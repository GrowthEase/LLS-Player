#include "rtd_frame_queue.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace rtd {

RtdFrameQueue::RtdFrameQueue(size_t capacity, size_t default_size)
    : capacity_(capacity), default_size_(default_size) {
  RTC_LOG(LS_INFO) << "RtdFrameQueue::RtdFrameQueue().";
}

RtdFrameQueue::~RtdFrameQueue() {
  RTC_LOG(LS_INFO) << "RtdFrameQueue::~RtdFrameQueue().";
  MutexLock lock(&mutex_);

  for (RtdFrameBuffer* buffer : queue_) {
    delete buffer;
  }
  for (RtdFrameBuffer* buffer : free_list_) {
    delete buffer;
  }
}

size_t RtdFrameQueue::Size() {
  MutexLock lock(&mutex_);
  return queue_.size();
}

void RtdFrameQueue::Clear() {
  MutexLock lock(&mutex_);
  while (!queue_.empty()) {
    free_list_.push_back(queue_.front());
    queue_.pop_front();
  }
}

bool RtdFrameQueue::ReadFront(RtdFrameBuffer*& buffer) {
  MutexLock lock(&mutex_);
  if (queue_.empty()) {
    return false;
  }

  RtdFrameBuffer* packet = queue_.front();
  queue_.pop_front();
  buffer = packet;

  return true;
}

void RtdFrameQueue::FreeBuffer(RtdFrameBuffer* buffer) {
    MutexLock lock(&mutex_);
    free_list_.push_back(buffer);
}

bool RtdFrameQueue::WriteBack(const void* data, size_t bytes,
                              uint64_t pts, uint64_t dts, 
                              int duration, int flag) {
  MutexLock lock(&mutex_);
  if (queue_.size() == capacity_) {
    return false;
  }

  RtdFrameBuffer* packet;
  if (!free_list_.empty()) {
    packet = free_list_.back();
    free_list_.pop_back();
    if (packet->buffer->capacity() < bytes) {
      RTC_LOG(LS_INFO) << "Current available buffer size is smaller than frame data size, recreate one.";
      delete packet->buffer;
      packet->buffer = new rtc::Buffer(bytes, default_size_);
    }
  } else {
    packet = new RtdFrameBuffer();
    packet->buffer = new rtc::Buffer(bytes, default_size_);
  }

  packet->buffer->SetData(static_cast<const uint8_t*>(data), bytes);
  packet->pts = pts;
  packet->dts = dts;
  packet->duration = duration;
  packet->flag = flag;
  queue_.push_back(packet);

  return true;
}

} // namespace rtd
} // namespace webrtc