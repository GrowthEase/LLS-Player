#include "rtd_engine_impl.h"
#include "rtd_def.h"

#include "api/create_peerconnection_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "modules/audio_device/include/fake_audio_device_impl.h"
#include "pc/session_description.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/message_digest.h"
#include "rtc_base/event_tracer.h"

namespace {

constexpr char kRtdSdkVersion[] = "v1.1.0";

} // namespace

namespace webrtc {
namespace rtd {

// SetSessionDescriptionObserver
class RtcSetLocalSessionDescriptionObserver : public SetSessionDescriptionObserver {
 public:
  static RtcSetLocalSessionDescriptionObserver* Create() {
    return new rtc::RefCountedObject<RtcSetLocalSessionDescriptionObserver>();
  }

  virtual void OnSuccess() { 
    RTC_LOG(LS_INFO) << "SetLocalDescription Success";
  }
  virtual void OnFailure(RTCError error) {
    RTC_LOG(LS_ERROR) << "SetLocalDescription failed: " << ToString(error.type()) << ": " << error.message();
  }

 protected:
  RtcSetLocalSessionDescriptionObserver() {}
  ~RtcSetLocalSessionDescriptionObserver() {}
};

class RtcSetRemoteSessionDescriptionObserver : public SetSessionDescriptionObserver {
 public:
  static RtcSetRemoteSessionDescriptionObserver* Create() {
    return new rtc::RefCountedObject<RtcSetRemoteSessionDescriptionObserver>();
  }

  virtual void OnSuccess() { 
    RTC_LOG(LS_INFO) << "SetRemoteDescription Success"; 
  }
  virtual void OnFailure(RTCError error) {
    RTC_LOG(LS_ERROR) << "SetRemoteDescription failed: " << ToString(error.type()) << ": " << error.message();
  }

 protected:
  RtcSetRemoteSessionDescriptionObserver() {}
  ~RtcSetRemoteSessionDescriptionObserver() {}
};

// CreateSessionDescriptionObserver
class RtcCreateSessionDescriptionObserver : public CreateSessionDescriptionObserver {
 public:
  static RtcCreateSessionDescriptionObserver* Create(RtdEngineImpl* rtd_engine) {
    return new rtc::RefCountedObject<RtcCreateSessionDescriptionObserver>(rtd_engine);
  }

  virtual void OnSuccess(SessionDescriptionInterface* desc) {
    RTC_LOG(LS_INFO) << __FUNCTION__;
    signal_set_local_description_(desc);
  }

  virtual void OnFailure(RTCError error) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " " << ToString(error.type()) << ": " << error.message();
  }

 public:
  sigslot::signal1<SessionDescriptionInterface*> signal_set_local_description_;

 protected:
  RtcCreateSessionDescriptionObserver(RtdEngineImpl* rtd_engine) {
    signal_set_local_description_.connect(rtd_engine, &RtdEngineImpl::SetLocalDescription);
  }
  ~RtcCreateSessionDescriptionObserver() {}
};

// RtdEngineImpl
RtdEngineImpl::RtdEngineImpl(
    RtdSinkInterface* sink,
    const std::string& url,
    RtdConf conf)
    : worker_thread_(nullptr),
      network_thread_(nullptr),
      signaling_thread_(nullptr),
      peer_connection_(nullptr),
      peer_connection_factory_(nullptr),
      conf_(conf),
      url_(url),
      sink_(sink),
      clock_(Clock::GetRealTimeClock()),
      signaling_(new RtdSignaling(url)),
      stream_info_parsed_(false),
      enable_audio_(true),
      enable_video_(true),
      sample_rate_(48000),
      channels_(2),
      start_open_time_ms_(0),
      first_video_frame_duration_(0),
      first_audio_frame_duration_(0),
      first_audio_frame_received_(false),
      first_video_frame_received_(false),
      media_conn_status_(RTD_MEDIA_CONN_NONE),
      stream_stopped_(false),
      is_stopped_(false) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::RtcEngineImpl() SDK_VERSION:" << kRtdSdkVersion;
}

RtdEngineImpl::~RtdEngineImpl() {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::~RtcEngineImpl()";
  sink_ = nullptr;
}

int RtdEngineImpl::Open() {
  RTC_LOG(LS_INFO) << "RtdEngineImpl::Open().";
  start_open_time_ms_ = clock_->TimeInMilliseconds();
  if (!InitializePeerConnection()) {
    RTC_LOG(LS_ERROR) << "InitializePeerConnection failed.";
    return RTD_ERROR_UNINITIALIZE;
  }

  if (!CreateOffer()) {
    RTC_LOG(LS_ERROR) << "CreateOffer failed.";
    return RTD_ERROR_UNINITIALIZE;
  }

  return RTD_ERROR_OPEN_SUCCESS;
}

void RtdEngineImpl::Close() {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::Close()";
  is_stopped_ = true;
  DeletePeerConnection();
}

bool RtdEngineImpl::InitializePeerConnection() {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::Init()";
  network_thread_ = rtc::Thread::CreateWithSocketServer();
  network_thread_->SetName("Network Thread", nullptr);
  if (!network_thread_->Start()) {
    RTC_LOG(LS_ERROR) << "network thread start failed.";
    return false;
  }

  worker_thread_ = rtc::Thread::Create();
  worker_thread_->SetName("Worker Thread", nullptr);
  if (!worker_thread_->Start()) {
    RTC_LOG(LS_ERROR) << "worker thread start failed.";
    return false;
  }

  signaling_thread_ = rtc::Thread::Create();
  signaling_thread_->SetName("Signaling Thread", nullptr);
  if (!signaling_thread_->Start()) {
    RTC_LOG(LS_ERROR) << "signaling thread start failed.";
    return false;
  }

  peer_connection_factory_ = CreatePeerConnectionFactory(network_thread_.get(), worker_thread_.get(), signaling_thread_.get(), 
                                                         rtc::make_ref_counted<FakeAudioDeviceImpl>(),
                                                         CreateBuiltinAudioEncoderFactory(), rtc::make_ref_counted<RtdAudioDecoderFactory>(this, this),
                                                         CreateBuiltinVideoEncoderFactory(), std::make_unique<RtdVideoDecoderFactory>(this),
														                             nullptr, nullptr);
  if (!peer_connection_factory_) {
    RTC_LOG(LS_ERROR) << "create peerconnection factory failed.";
    DeletePeerConnection();
    return false;
  }

  PeerConnectionFactoryInterface::Options option;
  option.disable_encryption = true;
  option.disable_network_monitor = true;
  peer_connection_factory_->SetOptions(option);

  if (!CreatePeerConnection()) {
    RTC_LOG(LS_ERROR) << "create peerconnection failed.";
    DeletePeerConnection();
    return false;
  }

  if (!AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO)) {
    RTC_LOG(LS_ERROR) << "add audio transceiver failed.";
    return false;
  }

  if (!AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO)) {
    RTC_LOG(LS_ERROR) << "add video transceiver failed.";
    return false;
  }

  return peer_connection_ != nullptr;
}

bool RtdEngineImpl::CreatePeerConnection() {
  RTC_DCHECK(peer_connection_factory_);
  RTC_DCHECK(!peer_connection_);
  RTC_LOG(LS_INFO) << "RtcEngineImpl::CreatePeerConnection()";
  PeerConnectionInterface::RTCConfiguration config;
  config.type =PeerConnectionInterface::IceTransportsType::kAll;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.audio_jitter_buffer_min_delay_ms = 500;
  config.audio_jitter_buffer_fast_accelerate = true;
  config.disable_link_local_networks = true;
  config.enable_dtls_srtp = false;
  auto result = peer_connection_factory_->CreatePeerConnectionOrError(config, PeerConnectionDependencies(this));
  if (!result.ok()) {
    peer_connection_ = nullptr;
    return false;
  }
  peer_connection_ = result.MoveValue();

  return true;
}

void RtdEngineImpl::DeletePeerConnection() {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::DeletePeerConnection()";
  peer_connection_ = nullptr;
  peer_connection_factory_ = nullptr;
}

bool RtdEngineImpl::AddTransceiver(cricket::MediaType type) {
  if (!signaling_thread_->IsCurrent()) {
    return signaling_thread_->Invoke<bool>(RTC_FROM_HERE, [this, &type] { return AddTransceiver(type); });
  }
  RTC_LOG(LS_INFO) << "RtcEngineImpl::AddTransceiver() type:" << type;

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kRecvOnly;
  auto rc = peer_connection_->AddTransceiver(type, init);
  if (!rc.ok()) {
    return false;
  }

  RtpTransceiverInterface* transceiver = rc.value();
  if (transceiver) {
    transceiver->receiver()->SetObserver(this);
  }

  return true;
}

bool RtdEngineImpl::CreateOffer() {
  if (!signaling_thread_->IsCurrent()) {
    return signaling_thread_->Invoke<bool>(RTC_FROM_HERE, [this] { return CreateOffer(); });
  }
  RTC_LOG(LS_INFO) << "RtcEngineImpl::CreateOffer()";

  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "peerconnection is nullptr.";
    return false;
  }

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = true;
  options.offer_to_receive_video = true;
  peer_connection_->CreateOffer(RtcCreateSessionDescriptionObserver::Create(this), options);
  return true;
}

void RtdEngineImpl::SetLocalDescription(SessionDescriptionInterface* desc) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::SetLocalDescription(), desc:" << desc;
  peer_connection_->SetLocalDescription(RtcSetLocalSessionDescriptionObserver::Create(), desc);
  if (desc->GetType() == SdpType::kOffer) {
    RTC_LOG(LS_INFO) << "desc is offer.";
    std::string sdp;
    desc->ToString(&sdp);
    OnSdpOffer(sdp);
  }
}

bool RtdEngineImpl::SetAnswer(const std::string& answer_sdp) {
  if (!signaling_thread_->IsCurrent()) {
    return signaling_thread_->Invoke<bool>(RTC_FROM_HERE, [this, &answer_sdp] { return SetAnswer(answer_sdp); });
  }
  RTC_LOG(LS_INFO) << "RtcEngineImpl::SetAnswer() sdp:" << answer_sdp;
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "peerconnection is nullptr.";
    return false;
  }
  SdpParseError error;
  SessionDescriptionInterface* session_description =
	  CreateSessionDescription(SessionDescriptionInterface::kAnswer, answer_sdp, &error);
  if (!session_description) {
    RTC_LOG(LS_ERROR) << "create answer description failed.";
    return false;
  }

  ParseStreamInfo(session_description);

  RTC_LOG(LS_INFO) << "RtcEngineImpl::SetRemoteDescription()";
  peer_connection_->SetRemoteDescription(RtcSetRemoteSessionDescriptionObserver::Create(), session_description);
  return true;
}

void RtdEngineImpl::ParseStreamInfo(SessionDescriptionInterface* session_description) {
  cricket::SessionDescription* sdec = session_description->description();
  for (auto content : sdec->contents()) {
    if (content.media_description()->type() == cricket::MEDIA_TYPE_AUDIO) {
      auto audio_content_desp = content.media_description()->as_audio();
      const cricket::AudioCodec& codec = audio_content_desp->codecs()[0];
      enable_audio_ = true;
      sample_rate_ = codec.clockrate;
      channels_ = codec.channels;
    } else if (content.media_description()->type() == cricket::MEDIA_TYPE_VIDEO) {
      enable_video_ = true;
    }
  }
  stream_info_parsed_ = true;
}

int RtdEngineImpl::GetStreamInfo(RtdDemuxInfo& info) {
  if (media_conn_status_ == RTD_MEDIA_CONN_FAILED) {
    return -1;
  }

  if (stream_info_parsed_ && media_conn_status_ == RTD_MEDIA_CONN_SUCCESS) {
    info.audio_enabled = enable_audio_;
    info.video_enabled = enable_video_;
    info.audio_sample_rate = sample_rate_;
    info.audio_channels = channels_;
    return 1;
  }

  return 0;
}

void RtdEngineImpl::CalcFirstVideoFrameDuration() {
  int64_t now_ms = clock_->TimeInMilliseconds();
  first_video_frame_duration_ = now_ms - start_open_time_ms_;
  RTC_LOG(LS_INFO) << "RtdEngineImpl::CalcFirstVideoFrameDuration() first_video_frame_duration:" << first_video_frame_duration_;
}

void RtdEngineImpl::CalcFirstAudioFrameDuration() {
  int64_t now_ms = clock_->TimeInMilliseconds();
  first_audio_frame_duration_ = now_ms - start_open_time_ms_;
  RTC_LOG(LS_INFO) << "RtdEngineImpl::CalcFirstAudioFrameDuration() first_audio_frame_duration:" << first_audio_frame_duration_;
}

int RtdEngineImpl::AudioDecoderInit(struct DecoderInitParam& init_param) {
  RTC_LOG(LS_INFO) << "RtdEngineImpl::AudioDecoderInit";
  if (conf_.callbacks.audio_decoder_init) {
    RtdAudioDecoderInitParam rtd_param;
    rtd_param.num_channels = init_param.ps_enabled ? (init_param.num_channels / 2) : init_param.num_channels;
    rtd_param.dec_hz = init_param.sbr_enabled ? (init_param.dec_hz / 2) : init_param.dec_hz;
    rtd_param.clockrate_hz = init_param.clockrate_hz;
    rtd_param.use_latm = init_param.use_latm;
    rtd_param.sbr_enabled = init_param.sbr_enabled;
    rtd_param.ps_enabled = init_param.ps_enabled;

    rtd_param.extra_data = init_param.extra_data.data();
    rtd_param.extra_data_len = init_param.extra_data.size();

    return conf_.callbacks.audio_decoder_init(conf_.ff_ctx, &rtd_param);
  }

  return -1;
}

int RtdEngineImpl::DecodeAudio(uint8_t* encoded, uint32_t encoded_len,
                               uint32_t sample_rate, uint32_t channels,
                               int16_t* decoded) {
  if (conf_.callbacks.audio_decode) {
    RtdAudioDecodedInfo info;
    info.encoded_data = encoded;
    info.encoded_size = encoded_len;
    info.dst_channels = channels;
    info.dst_sample_rate = sample_rate;

    int ret = conf_.callbacks.audio_decode(conf_.ff_ctx, &info, decoded);
    // RTC_LOG(LS_INFO) << "RtdEngineImpl::DecodeAudio call audio_decode ret:" << ret;
    return ret;
  }

  return 0;
}

int RtdEngineImpl::AudioDecoderUninit() {
  if (conf_.callbacks.audio_decoder_uninit) {
    conf_.callbacks.audio_decoder_uninit(conf_.ff_ctx);
  }

  return 0;
}

void RtdEngineImpl::OnSdpOffer(std::string& sdp) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::OnSdpOffer() sdp:" << sdp;
  if (signaling_) {
    std::string answer_sdp;
    int64_t now_ntp_ms = clock_->CurrentNtpInMilliseconds();
    std::string now_str = rtc::MD5(rtc::ToString(now_ntp_ms));
    RTC_LOG(LS_INFO) << "RtcEngineImpl::now_str:" << now_str;
    signaling_->ConnectAndWaitResponse(sdp, &answer_sdp);
    SetAnswer(answer_sdp);
  }
}

// PeerConnectionObserver implementation
void RtdEngineImpl::OnSignalingChange(PeerConnectionInterface::SignalingState new_state) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::OnSignalingChange() new_state:" << new_state;
}

void RtdEngineImpl::OnDataChannel(rtc::scoped_refptr<DataChannelInterface> data_channel) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::OnDataChannel().";
}

void RtdEngineImpl::OnIceConnectionChange(PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::OnIceConnectionChange new_state:" << new_state;
  switch (new_state) {
  case PeerConnectionInterface::kIceConnectionFailed:
  case PeerConnectionInterface::kIceConnectionDisconnected:
    media_conn_status_ = RTD_MEDIA_CONN_FAILED;
    break;
  case PeerConnectionInterface::kIceConnectionConnected:
    if (stream_info_parsed_) {
      RtdDemuxInfo info;
      info.audio_enabled = enable_audio_;
      info.video_enabled = enable_video_;
      info.audio_sample_rate = sample_rate_;
      info.audio_channels = channels_;
      if (conf_.callbacks.media_info_callback) {
        conf_.callbacks.media_info_callback(conf_.ff_ctx, info);
      }
    }
    break;
  case PeerConnectionInterface::kIceConnectionCompleted:
    media_conn_status_ = RTD_MEDIA_CONN_SUCCESS;
    break;
  case PeerConnectionInterface::kIceConnectionClosed:
	break;
  default:
    break;
  }
}

void RtdEngineImpl::OnConnectionChange(PeerConnectionInterface::PeerConnectionState new_state) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::OnConnectionChange() new_state:" << new_state;
}

void RtdEngineImpl::OnIceGatheringChange(PeerConnectionInterface::IceGatheringState new_state) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::OnIceGatheringChange() new_state:" << new_state;
}

void RtdEngineImpl::OnIceCandidate(const IceCandidateInterface* candidate) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::OnIceCandidate().";
}

void RtdEngineImpl::OnIceConnectionReceivingChange(bool receiving) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::OnIceConnectionReceivingChange().";
}

void RtdEngineImpl::OnFirstPacketReceived(cricket::MediaType media_type) {
  RTC_LOG(LS_INFO) << "RtcEngineImpl::OnFirstPacketReceived type:" << media_type;
}

// AudioFrameCallback implementation
int RtdEngineImpl::OnAudioFrame(AudioFrame* frame) {
  if (stream_stopped_) {
    return -1;
  }
  if (!first_audio_frame_received_) {
    CalcFirstAudioFrameDuration();
    first_audio_frame_received_ = true;
  }

  RtdAudioFrame audio_frame;
  audio_frame.data = const_cast<int16_t*>(frame->data());
  audio_frame.num_channels = frame->num_channels();
  audio_frame.samples_per_channel = frame->samples_per_channel();
  audio_frame.sample_rate_hz = frame->sample_rate_hz();
  audio_frame.timestamp_ms = timestamp_wraparound_handler_.Unwrap(frame->timestamp_) / (frame->sample_rate_hz() / 1000);
  audio_frame.timestamp_rtp = timestamp_wraparound_handler_.Unwrap(frame->timestamp_);
  audio_frame.codec_type = RtdAudioCodecType::RTD_OPUS;
  if (sink_) {
    sink_->OnAudioFrame(audio_frame);
  }

  return 0;
}

// EncodedImageCallback implementation
EncodedImageCallback::Result RtdEngineImpl::OnEncodedImage(const EncodedImage& encoded_image,
                                                           const CodecSpecificInfo* codec_specific_info) {
  if (!first_video_frame_received_) {
    CalcFirstVideoFrameDuration();
    first_video_frame_received_ = true;
  }
  auto result = EncodedImageCallback::Result(EncodedImageCallback::Result::OK, encoded_image.Timestamp());

  RtdVideoFrame frame;
  frame.timestamp_ms = timestamp_wraparound_handler_.Unwrap(encoded_image.Timestamp()) / 90;
  frame.play_timestamp_ms = frame.timestamp_ms; // no bframe
  frame.timestamp_rtp = timestamp_wraparound_handler_.Unwrap(encoded_image.Timestamp());
  frame.data = const_cast<uint8_t*>(encoded_image.data());
  frame.size = encoded_image.size();
  frame.codec_type = RtdVideoCodecType::RTD_H264;
  frame.frame_type = (encoded_image._frameType == VideoFrameType::kVideoFrameKey) ? 
                      RtdFrameType::RTD_KEY_FRAME : RtdFrameType::RTD_DELTA_FRAME;
  if (sink_) {
    sink_->OnVideoFrame(frame);
  }

  return result;
}

} // namespace rtd
} // namespace webrtc
