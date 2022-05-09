#include "rtd_demuxer.h"
#include "rtd_frame_queue.h"
#include "rtd_api.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace {

constexpr int kRtdAudioFrameLen = 960;        // 48000*0.01*2  10ms
constexpr int kRtdVideoFrameLen = 1382400;    // 1280*720*3/2  1 frame
constexpr int kRtdAudioBufCapacity = 800;     // about 5000ms
constexpr int kRtdVideoBufCapacity = 120;     // about 5000ms
constexpr int kRtdLogPrintInterval = 5000;    // 5000ms print once
constexpr int kAudioFrameDuration = 10;       // 10ms per audio frame

} // namespace

namespace webrtc {
namespace rtd {

RtdDemuxer::RtdDemuxer(RtdConf conf)
    : conf_(conf),
      video_queue_(new RtdFrameQueue(kRtdVideoBufCapacity, kRtdVideoFrameLen)),
      audio_queue_(new RtdFrameQueue(kRtdAudioBufCapacity, kRtdAudioFrameLen)),
      frame_(new RtdFrame()),
      current_frame_buffer_(nullptr),
      last_audio_receive_failed_(false),
      last_video_receive_failed_(false),
      iframe_requested_(false),
      audio_log_print_last_(0),
      video_log_print_last_(0),
      read_audio_frame_last_(0),
      read_video_frame_last_(0) {
  RTC_LOG(LS_INFO) << "RtdDemuxer::RtdDemuxer().";
}

RtdDemuxer::~RtdDemuxer() {
  RTC_LOG(LS_INFO) << "RtdDemuxer::~RtdDemuxer().";
}

int RtdDemuxer::Open(const std::string& url, const char* mode) {
  RTC_LOG(LS_INFO) << "RtdDemuxer::Open()";
  rtd_engine_ = RtdEngineInterface::Create(this, url, conf_);
  if (nullptr == rtd_engine_) {
    RTC_LOG(LS_ERROR) << "Failed to create Rtd Engine.";
    return RTD_ERROR_NULL_IS_PTR;
  }

  int ret = rtd_engine_->Open();
  if (ret != RTD_ERROR_OPEN_SUCCESS) {
    RTC_LOG(LS_ERROR) << "Fail to open url.";
  }

  return ret;
}

int RtdDemuxer::ReadFrame(RtdFrame*& frame) {
  // try to read video frame
  if (video_queue_ && video_queue_->ReadFront(current_frame_buffer_)) {
    int64_t now_ms = rtc::TimeMillis();
    if (now_ms - read_video_frame_last_ > kRtdLogPrintInterval) {
      RTC_LOG(LS_INFO) << "RtdDemuxer::ReadFrame: Read video frame, pts:" << current_frame_buffer_->pts << " type:" << current_frame_buffer_->flag;
      read_video_frame_last_ = now_ms;
    }
    if (frame_) {
      frame_->buf = current_frame_buffer_->buffer->data();
      frame_->size = (int)current_frame_buffer_->buffer->size();
      frame_->duration = current_frame_buffer_->duration;
      frame_->dts = current_frame_buffer_->dts;
      frame_->pts = current_frame_buffer_->pts;
      frame_->flag = current_frame_buffer_->flag;
      frame_->is_audio = 0;
      frame = frame_.get();
      return 0;
    } else {
      return 1;
    }
  } else {
    // try to read audio frame
    if (audio_queue_ && audio_queue_->ReadFront(current_frame_buffer_)) {
      int64_t now_ms = rtc::TimeMillis();
      if (now_ms - read_audio_frame_last_ > kRtdLogPrintInterval) {
        RTC_LOG(LS_INFO) << "RtdDemuxer::ReadFrame: Read audio frame, pts:" << current_frame_buffer_->pts;
        read_audio_frame_last_ = now_ms;
      }
      if (frame_) {
        frame_->buf = current_frame_buffer_->buffer->data();
        frame_->size = (int)current_frame_buffer_->buffer->size();
        frame_->duration = current_frame_buffer_->duration;
        frame_->dts = current_frame_buffer_->dts;
        frame_->pts = current_frame_buffer_->pts;
        frame_->is_audio = 1;
        frame = frame_.get();
        return 0;
      } else {
        return 1;
      }
    }
  }
  
  return 1;   // continue reading
}

int RtdDemuxer::Command(const char* cmd, void* arg) {
  if (strcmp(cmd, "getStreamInfo") == 0) { // Get meta data
    RtdDemuxInfo info = { 0 };
    if (rtd_engine_) {
      int ret = rtd_engine_->GetStreamInfo(info);
      if (ret == 1) {
        *((RtdDemuxInfo*)arg) = info;
      }
      return ret;
    }

    return -1;
  }

  return -1;
}

int RtdDemuxer::Close() {
  RTC_LOG(LS_INFO) << "RtdDemuxer::Close().";
  if (rtd_engine_) {
    rtd_engine_->Close();
    rtd_engine_.reset(nullptr);
  }
  return 0;
}

void RtdDemuxer::FreeFrame(RtdFrame* frame) {
  if (frame && current_frame_buffer_) {
    if (frame->is_audio) {
      audio_queue_->FreeBuffer(current_frame_buffer_);
    }
    else {
      video_queue_->FreeBuffer(current_frame_buffer_);
    }
    current_frame_buffer_ = nullptr;
  }
}

void RtdDemuxer::OnAudioFrame(const RtdAudioFrame& frame) {
  int64_t now_ms = rtc::TimeMillis();
  if (now_ms - audio_log_print_last_ > kRtdLogPrintInterval) {
    RTC_LOG(LS_INFO) << "Insert audio timestamp_ms:" << frame.timestamp_ms << " timestamp_rtp:" << frame.timestamp_rtp 
                     << " samplerate:" << frame.sample_rate_hz;
    audio_log_print_last_ = now_ms;
  }

  // 10ms pcm
  int size = frame.samples_per_channel * frame.num_channels * sizeof(int16_t);
  if (!audio_queue_->WriteBack(frame.data, size, frame.timestamp_ms, frame.timestamp_ms, kAudioFrameDuration)) {
    if (last_audio_receive_failed_) {   // reduce duplicated failing process
      return;
    }
    RTC_LOG(LS_ERROR) << "Failed to add audio frame to queue.";
    last_audio_receive_failed_ = true;
  } else {
    if (last_audio_receive_failed_) {
      last_audio_receive_failed_ = false;
    }
  }
}

void RtdDemuxer::OnVideoFrame(const RtdVideoFrame& frame) {
  int flag = (frame.frame_type == RtdFrameType::RTD_KEY_FRAME) ? 1 : 0;
  int64_t now_ms = rtc::TimeMillis();
  if (now_ms - video_log_print_last_ > kRtdLogPrintInterval) {
    RTC_LOG(LS_INFO) << "Insert video timestamp_ms:" << frame.timestamp_ms << " play_timestamp_ms:" 
                     << frame.play_timestamp_ms << " timestamp_rtp:" << frame.timestamp_rtp << " type:" << flag;
    video_log_print_last_ = now_ms;
  }

  if (iframe_requested_) {
    if (flag) {  // key frame
      iframe_requested_ = false;
      RTC_LOG(LS_INFO) << "First key frame arrived after requesting I-frame, begin to push to queue.";
    } else {
      RTC_LOG(LS_WARNING) << "Discard non-key frame after requesting I-frame.";
      return;
    }
  }
 
  if (!video_queue_->WriteBack(frame.data, frame.size, frame.play_timestamp_ms, frame.timestamp_ms, 0, flag)) {
    if (last_video_receive_failed_) {   // reduce duplicated failing process
      return;
    }

    RTC_LOG(LS_ERROR) << "Failed to add video frame to queue, discard current video packet.";
    iframe_requested_ = true;
    video_queue_->Clear();
    last_video_receive_failed_ = true;
  } else {
    if (last_video_receive_failed_) {
      last_video_receive_failed_ = false;
    }
  }
}

} // namespace rtd
} // namespace webrtc
