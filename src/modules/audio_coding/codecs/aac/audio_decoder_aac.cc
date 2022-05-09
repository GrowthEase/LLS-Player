#include "audio_decoder_aac.h"
#include "api/audio/audio_frame.h"
#include "rtc_base/logging.h"

namespace webrtc {

class AacFrame : public AudioDecoder::EncodedAudioFrame {
 public:
  AacFrame(AudioDecoderAacImpl* decoder,
           rtc::Buffer&& payload,
           bool is_primary_payload)
    : decoder_(decoder),
      payload_(std::move(payload)),
      is_primary_payload_(is_primary_payload) {}

  size_t Duration() const override {
    int ret;
    if (is_primary_payload_) {
      ret = decoder_->PacketDuration(payload_.data(), payload_.size());
    } else {
      ret = decoder_->PacketDurationRedundant(payload_.data(), payload_.size());
    }
    return (ret < 0) ? 0 : static_cast<size_t>(ret);
  }

  bool IsDtxPacket() const override { return payload_.size() <= 2; }

  absl::optional<DecodeResult> Decode(
    rtc::ArrayView<int16_t> decoded) const override {
    AudioDecoder::SpeechType speech_type = AudioDecoder::kSpeech;
    int ret;
    if (is_primary_payload_) {
      ret = decoder_->Decode(payload_.data(), payload_.size(), decoder_->SampleRateHz(), 
                             decoded.size()*sizeof(int16_t), decoded.data(), &speech_type);
    } else {
      ret = decoder_->DecodeRedundant(payload_.data(), payload_.size(), decoder_->SampleRateHz(), 
                                      decoded.size()*sizeof(int16_t), decoded.data(), &speech_type);
    }

    if (ret < 0) {
      return absl::nullopt;
    }

    return DecodeResult{static_cast<size_t>(ret), speech_type};
  }

 private:
  AudioDecoderAacImpl* const decoder_;
  const rtc::Buffer payload_;
  const bool is_primary_payload_;
};

AudioDecoderAacImpl::AudioDecoderAacImpl(AudioDecoderSink* sink,
                                         size_t num_channels,
                                         int dec_hz,
                                         int clockrate_hz,
                                         bool use_latm, 
                                         bool sbr_enabled,
                                         bool ps_enabled,
                                         uint8_t *extra_data,
                                         int extra_data_len)
    :  sink_(std::move(sink)) {
  RTC_LOG(LS_INFO) << "[AAC]AudioDecoderAacImpl::AudioDecoderAacImpl() ch:" << num_channels
                   << ", dec_hz:" << dec_hz
                   << ", clockrate_hz:" << clockrate_hz << ", use_latm:" << use_latm
                   << ", sbr:" << sbr_enabled << ", ps:" << ps_enabled
                   << ", extra_data_p:" << extra_data << ", extra_data_len:" << extra_data_len
                   << " sink_:" << sink_;

  init_param_.num_channels = num_channels;
  init_param_.dec_hz  = dec_hz;
  init_param_.clockrate_hz = clockrate_hz;
  init_param_.use_latm = use_latm;
  init_param_.sbr_enabled = sbr_enabled;
  init_param_.ps_enabled = ps_enabled;

  init_param_.extra_data.Clear();
  if (extra_data && extra_data_len) {
    init_param_.extra_data.SetData(extra_data, extra_data_len);
  }

  if (sink_) {
    sink_->AudioDecoderInit(init_param_);
  }
}

AudioDecoderAacImpl::~AudioDecoderAacImpl() {
  RTC_LOG(LS_INFO) << "AudioDecoderAacImpl::~AudioDecoderAacImpl()";
  if (sink_) {
    sink_->AudioDecoderUninit();
  }
}

std::vector<AudioDecoder::ParseResult> AudioDecoderAacImpl::ParsePayload(rtc::Buffer&& payload,
                                                                         uint32_t timestamp) {
  std::vector<ParseResult> results;
  std::unique_ptr<EncodedAudioFrame> frame(new AacFrame(this, std::move(payload), true));
  results.emplace_back(timestamp, 0, std::move(frame));
  return results;
}

int AudioDecoderAacImpl::PacketDuration(const uint8_t* encoded, size_t encoded_len) const {
  return 0;//static_cast<int>(encoded_len / Channels() / 2);
}

int AudioDecoderAacImpl::PacketDurationRedundant(const uint8_t* encoded,
                                                 size_t encoded_len) const {
  return PacketDuration(encoded, encoded_len);
}

void AudioDecoderAacImpl::Reset() {
  RTC_LOG(LS_INFO) << "AudioDecoderAacImpl::Reset()";
  if (sink_) {
    sink_->AudioDecoderInit(init_param_);
  }
}

int AudioDecoderAacImpl::SampleRateHz() const {
  return init_param_.clockrate_hz;
}

size_t AudioDecoderAacImpl::Channels() const {
  return init_param_.num_channels;
}

int AudioDecoderAacImpl::DecodeInternal(const uint8_t* encoded,
                                        size_t encoded_len,
                                        int sample_rate_hz,
                                        int16_t* decoded,
                                        SpeechType* speech_type) {
  // RTC_LOG(LS_INFO) << "AudioDecoderAacImpl::DecodeInternal encoded_len:" << encoded_len << " sample_rate_hz:" << sample_rate_hz;
  int ret = 0;
  int16_t temp_type = 1;  // Default is speech.
  uint8_t audio_encoded[2048] = {0};
  memcpy(audio_encoded, encoded, encoded_len);
  if (sink_) {
    ret = sink_->DecodeAudio(audio_encoded, encoded_len, sample_rate_hz, init_param_.num_channels, decoded);
  }
   if (ret > 0)
     ret *= static_cast<int>(init_param_.num_channels);  // Return total number of samples.
  *speech_type = ConvertSpeechType(temp_type);
  // RTC_LOG(LS_INFO) << "AudioDecoderAacImpl::DecodeInternal ret:" << ret;
  return ret;
}


} // namespace webrtc