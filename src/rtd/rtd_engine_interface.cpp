#include "rtd_engine_interface.h"
#include "rtd_engine_impl.h"

namespace webrtc {
namespace rtd {

std::unique_ptr<RtdEngineInterface> RtdEngineInterface::Create(
    RtdSinkInterface* sink,
    const std::string& url,
    RtdConf conf) {
  return std::unique_ptr<RtdEngineInterface>(new RtdEngineImpl(sink, url, conf));
}


} // namespace rtd
} // namespace webrtc