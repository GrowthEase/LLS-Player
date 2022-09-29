#ifndef RTD_ENGINE_INTERFACE_H_
#define RTD_ENGINE_INTERFACE_H_

#include <string>
#include <memory>
#include "rtd_def.h"
#include "rtd_internal.h"

namespace webrtc {
namespace rtd {

class RtdSinkInterface {
 public:
  virtual ~RtdSinkInterface() = default;

  virtual void OnAudioFrame(const RtdAudioFrame& frame) = 0;
  virtual void OnVideoFrame(const RtdVideoFrame& frame) = 0;
};

class RtdEngineInterface {
 public:
  RtdEngineInterface() = default;
  virtual ~RtdEngineInterface() = default;
  static std::unique_ptr<RtdEngineInterface> Create(
      RtdSinkInterface* sink,
      const std::string& url,
      RtdConf conf);

  virtual int Open() = 0;
  virtual void Close() = 0;
  virtual bool SetAnswer(const std::string& answer_sdp) = 0;
  virtual int GetStreamInfo(RtdDemuxInfo& info) = 0;
};

} // namespace rtd
} // namespace webrtc

#endif // !RTD_ENGINE_INTERFACE_H_
