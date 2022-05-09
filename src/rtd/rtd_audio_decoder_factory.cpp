#include "rtd_audio_decoder_factory.h"
#include "absl/strings/match.h"
#include "modules/audio_coding/codecs/aac/audio_decoder_aac.h"
#include "modules/audio_coding/codecs/opus/audio_decoder_opus.h"
#include "rtc_base/logging.h"
#include "rtd_def.h"

namespace webrtc {
namespace rtd {

class AudioDecoderOpusExternal final : public AudioDecoderOpusImpl {
 public:
  AudioDecoderOpusExternal(AudioFrameCallback* callback, int num_channels, int clockrate_hz)
      : AudioDecoderOpusImpl(num_channels, clockrate_hz),
        decoded_callback_(callback) {
    RTC_LOG(LS_INFO) << "AudioDecoderOpusExternal::AudioDecoderOpusExternal()";
  }

  ~AudioDecoderOpusExternal() override {
    RTC_LOG(LS_INFO) << "AudioDecoderOpusExternal::~AudioDecoderOpusExternal()";
  }

  void OnAudioFrame(AudioFrame* frame) const override {
    if (decoded_callback_ != nullptr) {
      decoded_callback_->OnAudioFrame(frame);
    }
  }

 private:
  AudioFrameCallback* decoded_callback_;
  RTC_DISALLOW_COPY_AND_ASSIGN(AudioDecoderOpusExternal);
};

class AudioDecoderAacExternal final : public AudioDecoderAacImpl {
 public:
  AudioDecoderAacExternal(AudioFrameCallback* callback, 
                          AudioDecoderSink* sink, 
                          int  num_channels,
                          int  dec_hz,
                          int  clockrate_hz,
                          bool use_latm,
                          bool sbr_enabled,
                          bool ps_enabled,
                          uint8_t* extra_data,
                          int extra_data_len)
    : AudioDecoderAacImpl(sink, num_channels, 
                          dec_hz,
                          clockrate_hz,
                          use_latm,
                          sbr_enabled, 
                          ps_enabled,
                          extra_data, 
                          extra_data_len),
                          decoded_callback_(callback) {
    RTC_LOG(LS_INFO) << "AudioDecoderAacExternal::AudioDecoderAacExternal() num_channels:" << num_channels << " clockrate_hz:" << clockrate_hz;
  }

  ~AudioDecoderAacExternal() override {
    RTC_LOG(LS_INFO) << "AudioDecoderAacExternal::~AudioDecoderAacExternal()";
  }

  void OnAudioFrame(AudioFrame* frame) const override {
    if (decoded_callback_ != nullptr) {
      decoded_callback_->OnAudioFrame(frame);
    }
  }

 private:
  AudioFrameCallback* decoded_callback_;
  RTC_DISALLOW_COPY_AND_ASSIGN(AudioDecoderAacExternal);
};

RtdAudioDecoderFactory::RtdAudioDecoderFactory(AudioFrameCallback* callback, AudioDecoderSink* sink)
    : decoded_callback_(callback),
      sink_(sink) {
  RTC_LOG(LS_INFO) << "RtdAudioDecoderFactory::RtdAudioDecoderFactory()";
}

RtdAudioDecoderFactory::~RtdAudioDecoderFactory() {
  RTC_LOG(LS_INFO) << "RtdAudioDecoderFactory::~RtdAudioDecoderFactory()";
}

std::vector<AudioCodecSpec> RtdAudioDecoderFactory::GetSupportedDecoders() {
  std::vector<AudioCodecSpec> codec_specs;
  AudioCodecInfo info[] = {{48000, 2, 64000, 6000, 510000},
                           {44100, 2, 64000, 6000, 510000}};
  AudioCodecSpec codec_spec[] = {
          {{"MP4A-LATM", 48000, 2}, info[0]},
          {{"MP4A-LATM", 44100, 2}, info[1]},
          {{"MP4A-ADTS", 48000, 2}, info[0]},
          {{"MP4A-ADTS", 44100, 2}, info[1]},
          {{"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}}, info[0]}};

  for (auto spec : codec_spec) {
    codec_specs.push_back(spec);
  }
  
  return codec_specs;
}

bool RtdAudioDecoderFactory::IsSupportedDecoder(const SdpAudioFormat& format) {
  return true;
}

std::unique_ptr<AudioDecoder> 
RtdAudioDecoderFactory::MakeAudioDecoder(const SdpAudioFormat& format,
                                         absl::optional<AudioCodecPairId> codec_pair_id) {
  RTC_LOG(LS_INFO) << "MakeAudioDecoder format.name:" << format.name 
                   << " channels:" << format.num_channels
                   << " clockrate_hz:" << format.clockrate_hz;

  if (absl::EqualsIgnoreCase(format.name, "opus")) {
    return std::make_unique<AudioDecoderOpusExternal>(decoded_callback_, format.num_channels, format.clockrate_hz);
  } else if (absl::EqualsIgnoreCase(format.name, "MP4A-ADTS")) {
    return std::make_unique<AudioDecoderAacExternal>(decoded_callback_, 
                                                     sink_, 
                                                     format.num_channels, 
                                                     format.clockrate_hz,
                                                     format.clockrate_hz,
                                                     false, false, false,
                                                     nullptr, 0);
  } else if (absl::EqualsIgnoreCase(format.name, "MP4A-LATM")) {
    size_t channnel = format.num_channels;
    size_t dec_hz = format.clockrate_hz;
    size_t clockrate_hz = format.clockrate_hz;
    
    bool is_cprsented = false;
    bool sbr_enabled = false;
    bool ps_enabled = false;
    rtc::BufferT<uint8_t> extra_data;
    is_cprsented = format.IsCPresented();
    if (!is_cprsented) {
      if (!format.GetInfoFromConfig(channnel, dec_hz, sbr_enabled, ps_enabled, extra_data)) {
        RTC_LOG(LS_ERROR) << "[AAC][LATM]MakeAudioDecoder()  format.GetInfoFromConfig() err";
      }

      //for Exception
      if (0 == channnel || 0 == dec_hz) {
        RTC_LOG(LS_ERROR) << "[AAC][LATM]MakeAudioDecoder() check it: "
                          << "why get exception value of channel(" << channnel << ") "
                          << "and sample_rate(" << dec_hz << ")";
        channnel = format.num_channels;
        dec_hz = format.clockrate_hz; 
      }
    }

    std::ostringstream oss;
    oss << "[AAC]DecoderFactory, before new codec, name:" << format.name << ", "
        << "ch:" << channnel << ", dec_hz:"<< dec_hz <<", clockrate_hz:" << clockrate_hz << ", "
        << "cp:" << is_cprsented << ", ";
    if (!is_cprsented) {
      oss << "sbr:" << sbr_enabled << ", ps:" << ps_enabled << ", extra_data_size:" << extra_data.size();
    } else {
      oss << "no srb/ps info when cp=1";
    }
    RTC_LOG(LS_INFO) << oss.str();

    return std::make_unique<AudioDecoderAacExternal>(decoded_callback_, sink_, channnel, 
                                                     dec_hz, clockrate_hz,
                                                     is_cprsented, sbr_enabled, ps_enabled,
                                                     extra_data.data(), extra_data.size());
  }

  return nullptr;
}

} // namespace rtd
} // namespace webrtc