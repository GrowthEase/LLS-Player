#ifndef RTD_VIDEO_DECODER_FACTORY_H_
#define RTD_VIDEO_DECODER_FACTORY_H_

#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"

namespace webrtc {
namespace rtd {

class RtdVideoDecoderFactory : public VideoDecoderFactory {
 public:
  RtdVideoDecoderFactory(EncodedImageCallback* callback);

  ~RtdVideoDecoderFactory() override;

  std::vector<SdpVideoFormat> GetSupportedFormats() const override;

  std::unique_ptr<VideoDecoder> CreateVideoDecoder(const SdpVideoFormat& format) override;

 private:
  EncodedImageCallback* encoded_callback_;
};

} // namespace rtd
} // namespace webrtc

#endif // !RTD_VIDEO_DECODER_FACTORY_H_