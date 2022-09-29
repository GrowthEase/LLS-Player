#include "rtd_video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/codecs/h264/include/h264.h"

namespace webrtc {
namespace rtd {

class RtdDummyVideoDecoder : public VideoDecoder {
 public:
  RtdDummyVideoDecoder(EncodedImageCallback* callback)
    : encoded_callback_(callback) {
  }

  ~RtdDummyVideoDecoder() override {
  }

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    if (encoded_callback_ != nullptr) {
      auto result = encoded_callback_->OnEncodedImage(input_image, nullptr);
      if (result.error == EncodedImageCallback::Result::Error::OK) {

      } else if (result.error == EncodedImageCallback::Result::Error::ERROR_SEND_FAILED) {

      }
    }
    return 0;
  }

  int32_t InitDecode(const VideoCodec* codec_settings, int32_t number_of_cores) override {
    return 0;
  }

  bool Configure(const Settings& settings) override {
    return true;
  }

  int32_t RegisterDecodeCompleteCallback(DecodedImageCallback* callback) override {
    return 0;
  }

  int32_t Release() override {
    return 0;
  }
 private:
  EncodedImageCallback* encoded_callback_;
};

RtdVideoDecoderFactory::RtdVideoDecoderFactory(EncodedImageCallback* callback)
    : encoded_callback_(callback) {
}

RtdVideoDecoderFactory::~RtdVideoDecoderFactory() {}

std::vector<SdpVideoFormat> RtdVideoDecoderFactory::GetSupportedFormats() const {
  std::vector<SdpVideoFormat> supported_codecs;
  for (const SdpVideoFormat& format : SupportedH264Codecs())
    supported_codecs.push_back(format);
  return supported_codecs;
}

std::unique_ptr<VideoDecoder> RtdVideoDecoderFactory::CreateVideoDecoder(const SdpVideoFormat& format) {
  if (format.name == "H264") {
    return std::make_unique<RtdDummyVideoDecoder>(encoded_callback_);
  }

  return nullptr;  
}

} // namspace rtd
} // namespace webrtc
