#ifndef RTD_ENGINE_IMPL_H_
#define RTD_ENGINE_IMPL_H_

#include "api/peer_connection_interface.h"
#include "api/rtp_receiver_interface.h"
#include "rtc_base/async_invoker.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/time_utils.h"
#include "rtd_engine_interface.h"
#include "rtd_signaling.h"
#include "rtd_audio_decoder_factory.h"
#include "rtd_video_decoder_factory.h"

namespace webrtc {
namespace rtd {

class RtdEngineImpl : public PeerConnectionObserver,
                      public RtpReceiverObserverInterface,
                      public RtdEngineInterface,
                      public AudioFrameCallback, 
                      public EncodedImageCallback,
                      public AudioDecoderSink,
                      public sigslot::has_slots<sigslot::multi_threaded_local> {
 public:
  RtdEngineImpl(RtdSinkInterface* sink,
                const std::string& url,
                RtdConf conf);
  virtual ~RtdEngineImpl();

  // RtdEngineInterface implementation
  int Open() override;
  void Close() override;
  bool SetAnswer(const std::string& answer_sdp) override;
  int GetStreamInfo(RtdDemuxInfo& info) override;

  bool CreateOffer();
  void SetLocalDescription(SessionDescriptionInterface* desc);

 protected:
  bool InitializePeerConnection();
  bool CreatePeerConnection();
  void DeletePeerConnection();
  bool AddTransceiver(cricket::MediaType type);
  bool WaitSyncEvent(int ms);
  void SignalSyncEvent(bool success);
  void CalcFirstVideoFrameDuration();
  void CalcFirstAudioFrameDuration();

  // AudioDecoderSink implementation
  int AudioDecoderInit(struct DecoderInitParam& init_param) override;
  int DecodeAudio(uint8_t* encoded, uint32_t encoded_len,
                  uint32_t sample_rate, uint32_t channels,
                  int16_t* decoded) override;
  int AudioDecoderUninit() override;

  // PeerConnectionObserver implementation
  void OnSignalingChange(PeerConnectionInterface::SignalingState new_state) override;
  void OnDataChannel(rtc::scoped_refptr<DataChannelInterface> data_channel) override;
  void OnIceConnectionChange(PeerConnectionInterface::IceConnectionState new_state) override;
  void OnConnectionChange(PeerConnectionInterface::PeerConnectionState new_state) override;
  void OnIceGatheringChange(PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override;

  // RtpReceiverObserverInterface implementation
  void OnFirstPacketReceived(cricket::MediaType media_type) override;

  // AudioFrameCallback implementation
  int OnAudioFrame(AudioFrame* frame) override;

  // EncodedImageCallback implementation
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info) override;

  void OnSdpOffer(std::string& sdp);

  void ParseStreamInfo(SessionDescriptionInterface* session_description);

 private:
  std::unique_ptr<rtc::Thread> worker_thread_;
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> signaling_thread_;

  rtc::scoped_refptr<PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> peer_connection_factory_;

  RtdConf conf_;
  std::string url_;
  RtdSinkInterface* sink_;
  Clock* const clock_;
  //rtc::AsyncInvoker invoker_;
  rtc::TimestampWrapAroundHandler timestamp_wraparound_handler_;
  std::unique_ptr<RtdSignaling> signaling_;
  bool stream_info_parsed_;
  bool enable_audio_;
  bool enable_video_;
  int sample_rate_;
  int channels_;
  int64_t start_open_time_ms_;
  int64_t first_video_frame_duration_;
  int64_t first_audio_frame_duration_;
  bool first_audio_frame_received_;
  bool first_video_frame_received_;
  RtdMediaConnStatus media_conn_status_;
  bool stream_stopped_;
  bool is_stopped_;
};

} // namespace rtd
} // namespace webrtc

#endif // !RTD_ENGINE_IMPL_H_