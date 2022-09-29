#ifndef RTD_DEMUXER_H_
#define RTD_DEMUXER_H_

#include "rtd_engine_interface.h"
#include "rtd_frame_queue.h"
#include "rtd_def.h"
namespace webrtc {
namespace rtd {

class RtdDemuxer : public RtdSinkInterface {
 public:
  RtdDemuxer(RtdConf conf);
  ~RtdDemuxer();
  int Open(const std::string& url, const char* mode = "r");
  int ReadFrame(RtdFrame*& frame);

  // set/get parameters
  int Command(const char* cmd, void* arg);
  int Close();
  void FreeFrame(RtdFrame* frame);

  // RtdSinkInterface implementation
  void OnAudioFrame(const RtdAudioFrame& frame) override;
  void OnVideoFrame(const RtdVideoFrame& frame) override;

 private:
  std::unique_ptr<RtdEngineInterface> rtd_engine_;
  RtdConf conf_;
  std::unique_ptr<RtdFrameQueue> video_queue_;
  std::unique_ptr<RtdFrameQueue> audio_queue_;
  std::unique_ptr<RtdFrame> frame_;
  RtdFrameBuffer* current_frame_buffer_;
  bool last_audio_receive_failed_;
  bool last_video_receive_failed_;

  bool iframe_requested_;
  int64_t audio_log_print_last_;
  int64_t video_log_print_last_;
  int64_t read_audio_frame_last_;
  int64_t read_video_frame_last_;
};

} // namespace rtd
} // namespace webrtc

#endif // !RTD_DEMUXER_H_


