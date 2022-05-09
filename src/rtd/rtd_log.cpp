#include "rtd_log.h"

namespace webrtc {
namespace rtd {

RtdLogSink::RtdLogSink(RtdConf conf)
    : conf_(conf) {
}

RtdLogSink::~RtdLogSink() {
}

void RtdLogSink::OnLogMessage(const std::string& msg,
                              rtc::LoggingSeverity severity,
                              const char* tag) {
  if (conf_.callbacks.log_sink) {
    conf_.callbacks.log_sink(conf_.ff_ctx, severity, msg.c_str());
  }
}

void RtdLogSink::OnLogMessage(const std::string& message,
                              rtc::LoggingSeverity severity) {
  if (conf_.callbacks.log_sink) {
    conf_.callbacks.log_sink(conf_.ff_ctx, severity, message.c_str());
  }
}

void RtdLogSink::OnLogMessage(const std::string& message) {
  if (conf_.callbacks.log_sink) {
    conf_.callbacks.log_sink(conf_.ff_ctx, rtc::LS_INFO, message.c_str());
  }
}

} // namespace rtd
} // namespace webrtc

