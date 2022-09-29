#ifndef RTD_API_IMPL_H_
#define RTD_API_IMPL_H_

#include <string>
#include "rtd_def.h"
#include "rtd_demuxer.h"
#include "rtd_log.h"

namespace webrtc {
namespace rtd {

class RtdApiImpl {
 public:
  RtdApiImpl(RtdConf conf);
  virtual ~RtdApiImpl();

  bool Initialize();
  int Start(std::string& url);
  bool Stop();

  int ReadFrame(RtdFrame*& frame);
  void FreeFrame(RtdFrame* frame);
  // set/get parameters
  int Command(const char* cmd, void* arg);

private:
  void InitLog(RtdConf conf);
  RtdConf conf_;
  std::unique_ptr<RtdLogSink> rtd_log_sink_;
  std::unique_ptr<RtdDemuxer> demuxer_;
  RTC_DISALLOW_COPY_AND_ASSIGN(RtdApiImpl);
};

} // namespace rtd
} // namespace webrtc

#endif // !RTD_API_IMPL_H_

