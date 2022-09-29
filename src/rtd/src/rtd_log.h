#ifndef RTD_LOG_H_
#define RTD_LOG_H_

#include "rtc_base/logging.h"
#include "rtd_def.h"

namespace webrtc {
namespace rtd {

class RtdLogSink : public rtc::LogSink {
 public:
  RtdLogSink(RtdConf conf);
  virtual ~RtdLogSink();

  virtual void OnLogMessage(const std::string& msg,
                            rtc::LoggingSeverity severity,
                            const char* tag) override;
  virtual void OnLogMessage(const std::string& message,
                            rtc::LoggingSeverity severity) override;
  virtual void OnLogMessage(const std::string& message) override;

private:
    RtdConf conf_;
};

} // namespace rtd
} // namespace webrtc


#endif // !RTD_LOG_H_
