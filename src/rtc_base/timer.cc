
#include "timer.h"

#include <chrono>
#include <thread>
#ifdef WEBRTC_WIN
#include <winsock.h>
#else
#include <sys/time.h>
#endif


namespace rtc {

namespace internal {

uint64_t getNowTimeMicrosecond() {
  auto now_time = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(now_time)
      .count();
}

class SelectTimer : public Timer {
 public:
  SelectTimer(uint64_t intervalMicroseconds);

  ~SelectTimer() override;

  void WaitUntilNext() override;

  void start() override;

 private:
  uint64_t interval_microseconds_;
  uint64_t start_time_point_;
  uint64_t next_time_point_;
};

SelectTimer::SelectTimer(uint64_t intervalMicroseconds)
    : interval_microseconds_(intervalMicroseconds),
      start_time_point_(getNowTimeMicrosecond()),
      next_time_point_(getNowTimeMicrosecond()) {}

void SelectTimer::WaitUntilNext() {
  uint64_t now_time = getNowTimeMicrosecond();
  if (now_time < next_time_point_) {
    auto diff = next_time_point_ - now_time;
#ifdef WEBRTC_WIN
    std::this_thread::sleep_for(std::chrono::microseconds(diff));
#else
    struct timeval timeout;
    timeout.tv_sec = diff / 1000000;
    timeout.tv_usec = diff % 1000000;
    int ret = select(0, nullptr, nullptr, nullptr, &timeout);
    if (ret != 0) {
    }
#endif
  }

  next_time_point_ += interval_microseconds_;
}

SelectTimer::~SelectTimer() {
  
}

void SelectTimer::start() {
  next_time_point_ = getNowTimeMicrosecond();
}
}  // namespace internal

std::unique_ptr<Timer> Timer::CreateTimer(uint64_t intervalMicroseconds) {
  return std::make_unique<internal::SelectTimer>(intervalMicroseconds);
}

}  // namespace rtc