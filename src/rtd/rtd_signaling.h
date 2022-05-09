#ifndef RTD_SIGNALING_H_
#define RTD_SIGNALING_H_

#include <memory>
#include "third_party/http/src/rtd_http.h"

namespace webrtc {
namespace rtd {

class RtdEngineInterface;
class RtdSignaling {
 public:
  RtdSignaling(const std::string& url);
  ~RtdSignaling();

  void SetId(std::string& id) { request_id_ = id; }
  int ConnectAndWaitResponse(std::string& offer_sdp, std::string* answer_sdp);
  std::string GetId();

 private:
  std::unique_ptr<RtdHttp> http_;
  std::string url_;
  std::string server_domain_;
  std::string request_id_;
  std::string cid_;
  std::string uid_;
  int timeout_ms_;
};

} // namespace rtd
} // namespace webrtc

#endif // !RTD_SIGNALING_H_