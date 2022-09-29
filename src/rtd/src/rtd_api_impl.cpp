#include "rtd_api_impl.h"

namespace webrtc {
namespace rtd {

RtdApiImpl::RtdApiImpl(RtdConf conf)
    : conf_(conf),
      demuxer_(nullptr) {
  InitLog(conf_);
  RTC_LOG(LS_INFO) << "RtdApiImpl::RtdApiImpl().";
}

RtdApiImpl::~RtdApiImpl() {
  RTC_LOG(LS_INFO) << "RtdApiImpl::~RtdApiImpl().";
  demuxer_.reset(nullptr);
  rtc::LogMessage::RemoveLogToStream(rtd_log_sink_.get());
  rtd_log_sink_.reset();
}

bool RtdApiImpl::Initialize() {
  RTC_LOG(LS_INFO) << "RtdApiImpl::Initialize().";
  demuxer_.reset(new RtdDemuxer(conf_));
  if (!demuxer_) {
    RTC_LOG(LS_ERROR) << "initialize failed, create demuxer error.";
    return false;
  }

  return true;
}

int RtdApiImpl::Start(std::string& url) {
  RTC_LOG(LS_INFO) << "RtdApiImpl::Start().";
  if (!demuxer_) {
    RTC_LOG(LS_ERROR) << "start failed, demuxer is null.";
    return RTD_ERROR_NULL_IS_PTR;
  }

  int ret = demuxer_->Open(url, "r"); 
  if (ret != RTD_ERROR_OPEN_SUCCESS) {
    RTC_LOG(LS_ERROR) << "Open rtd url failed, url:" << url;
    demuxer_->Close();
  }

  return ret;
}

bool RtdApiImpl::Stop() {
  RTC_LOG(LS_INFO) << "RtdApiImpl::Stop().";
  if (demuxer_) {
    demuxer_->Close();
    return true;
  }

  return false;
}

int RtdApiImpl::ReadFrame(RtdFrame*& frame) {
  if (demuxer_) {
    return demuxer_->ReadFrame(frame);
  }
  return 0;
}

void RtdApiImpl::FreeFrame(RtdFrame* frame) {
  if (demuxer_) {
    demuxer_->FreeFrame(frame);
  }
}

int RtdApiImpl::Command(const char* cmd, void* arg) {
  if (demuxer_) {
    return demuxer_->Command(cmd, arg);
  }
  return 0;
}

void RtdApiImpl::InitLog(RtdConf conf) {
  rtd_log_sink_.reset(new RtdLogSink(conf));
  RtdLogLevel level = conf.log_level;
  rtc::LoggingSeverity ls;
  switch (level) {
  case RTD_LOG_NONE:
    ls = rtc::LS_NONE;
    break;
  case RTD_LOG_ERROR:
    ls = rtc::LS_ERROR;
    break;
  case RTD_LOG_DEBUG:
    ls = rtc::LS_VERBOSE;
    break;
  case RTD_LOG_WARN:
    ls = rtc::LS_WARNING;
    break;
  case RTD_LOG_INFO:
    ls = rtc::LS_INFO;
    break;
  default:
    ls = rtc::LS_INFO;
  }

  rtc::LogMessage::LogThreads(true);
  rtc::LogMessage::LogTimestamps(true);
  rtc::LogMessage::AddLogToStream(rtd_log_sink_.get(), ls);

  RTC_LOG(LS_INFO) << "RtdApiImpl::InitLog log level:" << level;
}

} // namespace rtd
} // namespace webrtc
