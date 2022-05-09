#ifndef MODULES_AUDIO_CODING_CODECS_AUDIO_DECODER_AAC_H_
#define MODULES_AUDIO_CODING_CODECS_AUDIO_DECODER_AAC_H_

#include "api/audio_codecs/audio_decoder_sink.h"
#include "api/audio_codecs/audio_decoder.h"
#include "rtc_base/constructor_magic.h"
#include <stdio.h>
namespace webrtc {

class AudioDecoderAacImpl : public AudioDecoder {
 public:
  AudioDecoderAacImpl(AudioDecoderSink* sink,
                      size_t num_channels,
                      int dec_hz,
                      int clockrate_hz,
                      bool use_latm,
                      bool sbr_enabled,
                      bool ps_enabled,
                      uint8_t* extra_data,
                      int extra_data_len);
  ~AudioDecoderAacImpl() override;

  std::vector<ParseResult> ParsePayload(rtc::Buffer&& payload,
                                        uint32_t timestamp) override;

  int PacketDuration(const uint8_t* encoded, size_t encoded_len) const override;

  int PacketDurationRedundant(const uint8_t* encoded,
                              size_t encoded_len) const override;

  void Reset() override;

  int SampleRateHz() const override;

  size_t Channels() const override;

  void SetPacketSampleRateHz(int sample_rate_hz) { packet_sample_rate_hz_ = sample_rate_hz; }

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  //const size_t channels_;
  //const int sample_rate_hz_;
  int packet_sample_rate_hz_;
  AudioDecoderSink* sink_;
  struct DecoderInitParam init_param_;
  RTC_DISALLOW_COPY_AND_ASSIGN(AudioDecoderAacImpl);
};

} // namespace webrtc

#endif // !MODULES_AUDIO_CODING_CODECS_AUDIO_DECODER_AAC_H_