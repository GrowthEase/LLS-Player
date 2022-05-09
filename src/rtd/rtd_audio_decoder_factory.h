#ifndef RTD_AUDIO_DECODER_FACTORY_H_
#define RTD_AUDIO_DECODER_FACTORY_H_

#include <memory>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_decoder_sink.h"
#include "api/audio/audio_frame.h"

namespace webrtc {
namespace rtd {

class AudioFrameCallback {
 public:
  virtual int OnAudioFrame(AudioFrame* frame) = 0;

 protected:
  virtual ~AudioFrameCallback() {}
};

class RtdAudioDecoderFactory : public AudioDecoderFactory {
 public:
  RtdAudioDecoderFactory(AudioFrameCallback* callback, AudioDecoderSink* sink);

  ~RtdAudioDecoderFactory() override;

  std::vector<AudioCodecSpec> GetSupportedDecoders() override;

  bool IsSupportedDecoder(const SdpAudioFormat& format) override;

  std::unique_ptr<AudioDecoder> MakeAudioDecoder(const SdpAudioFormat& format,
                                                 absl::optional<AudioCodecPairId> codec_pair_id) override;
 private:
  AudioFrameCallback* decoded_callback_;
  AudioDecoderSink* sink_;
};

} // namespace rtd
} // namespace webrtc

#endif // !RTD_AUDIO_DECODER_FACTORY_H_