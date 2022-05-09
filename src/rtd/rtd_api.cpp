#include "rtd_api.h"
#include "rtd_api_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace webrtc {
namespace rtd {

void* RtdCreate(RtdConf conf) {
  RTC_LOG(LS_INFO) << "RtdCreate.";
  RtdApiImpl* rtd = new RtdApiImpl(conf);
  if (!rtd) {
    RTC_LOG(LS_ERROR) << "RtdCreate failed.";
    return nullptr;
  }

  return rtd;
}

int RtdOpenStream(void* handle, const char* url, const char* mode) {
  RTC_LOG(LS_INFO) << "RtdOpenStream.";
  RtdApiImpl* rtd = static_cast<RtdApiImpl*>(handle);
  if (!rtd) {
    RTC_LOG(LS_ERROR) << "rtd open failed, rtd is null.";
    return RTD_ERROR_NULL_IS_PTR;
  }

  if (!rtd->Initialize()) {
    RTC_LOG(LS_ERROR) << "Initialize rtd failed.";
    return RTD_ERROR_UNINITIALIZE;
  }

  std::string url_str(url);
  int ret = rtd->Start(url_str);
  if (ret != RTD_ERROR_OPEN_SUCCESS) {
    RTC_LOG(LS_ERROR) << "Open rtd url failed, url:" << url;
  }

  return ret;
}

void RtdCloseStream(void* handle) {
  RTC_LOG(LS_INFO) << "RtdCloseStream.";
  RtdApiImpl* rtd = static_cast<RtdApiImpl*>(handle);
  if (rtd) {
    rtd->Stop();
    delete rtd;
    rtd = nullptr;
  }
}

int RtdCommand(void* handle, const char* cmd, void* arg) {
  RTC_LOG(LS_INFO) << "RtdCommand cmd:" << cmd;
  RtdApiImpl* rtd = static_cast<RtdApiImpl*>(handle);
  if (rtd) {
    return rtd->Command(cmd, arg);
  }
  return -1;
}

int RtdReadFrame(struct RtdFrame** frame, void* handle) {
  RtdApiImpl* rtd = static_cast<RtdApiImpl*>(handle);
  if (rtd) {
    return rtd->ReadFrame(*frame);
  }
  return -1;
}

void RtdFreeFrame(struct RtdFrame* frame, void* handle) {
  RtdApiImpl* rtd = static_cast<RtdApiImpl*>(handle);
  if (rtd) {
    rtd->FreeFrame(frame);
  }
}

const struct RtdApiFuncs* GetRtdApiFuncs(int version) {
  static RtdApiFuncs funcs;
  if (!funcs.create) {
    funcs.version = 0;
    funcs.create = RtdCreate;
    funcs.open = RtdOpenStream;
    funcs.command = RtdCommand;
    funcs.close = RtdCloseStream;
    funcs.read = RtdReadFrame;
    funcs.free_frame = RtdFreeFrame;
  }
  return &funcs;
}

} // namespace rtd
} // namespace webrtc

#ifdef __cplusplus
}
#endif