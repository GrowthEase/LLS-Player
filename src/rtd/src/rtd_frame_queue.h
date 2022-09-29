#ifndef RTD_FRAME_QUEUE_H_
#define RTD_FRAME_QUEUE_H_

#include <stddef.h>
#include <deque>
#include <vector>

#include "rtc_base/buffer.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace rtd {

struct RtdFrameBuffer {
  rtc::Buffer* buffer;
  uint64_t pts;           // presentation timestamp, in ms
  uint64_t dts;           // decoding timestamp, in ms
  int flag;               // for video frame
                          //     bit 0: key frame;
  int duration;           // in ms

  RtdFrameBuffer() { buffer = nullptr; }
  ~RtdFrameBuffer() {
    delete buffer;
  }
};

class RtdFrameQueue {
 public:
  // Creates a buffer queue with a given capacity and default buffer size.
  RtdFrameQueue(size_t capacity, size_t default_size);
  virtual ~RtdFrameQueue();

  // Return number of queued buffers.
  size_t Size();

  // Clear the BufferQueue by moving all Buffers from |queue_| to |free_list_|.
  void Clear();

  // ReadFront will only read one buffer at a time
  // Returns true unless no data could be returned.
  bool ReadFront(RtdFrameBuffer* & buffer);

  // Return current buffer to free list
  void FreeBuffer(RtdFrameBuffer* buffer);

  // WriteBack always writes either the complete memory or nothing.
  // pts: presentation time stamp, in ms
  // dts: decoding time stamp, in ms
  // duration: frame duration in ms
  // flag: for video frame, see flag in RtdFrameBuffer
  // Returns true unless no data could be written.
  bool WriteBack(const void* data, size_t bytes, uint64_t pts, uint64_t dts, int duration, int flag = 0);

 private:
  size_t capacity_;
  size_t default_size_;
  mutable Mutex mutex_;
  std::deque<RtdFrameBuffer*> queue_ RTC_GUARDED_BY(mutex_);
  std::vector<RtdFrameBuffer*> free_list_ RTC_GUARDED_BY(mutex_);

  //RTC_DISALLOW_COPY_AND_ASSIGN(RtdFrameQueue);
};

} // namespace rtd
} // namespace webrtc

#endif // !RTD_FRAME_QUEUE_H_