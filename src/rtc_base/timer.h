#ifndef RTC_BASE_TIMER_H_
#define RTC_BASE_TIMER_H_

#include <memory>

namespace rtc {

class Timer {
 public:
  static std::unique_ptr<Timer> CreateTimer(uint64_t intervalMicroseconds);

  virtual ~Timer() {}

  virtual void WaitUntilNext() = 0;

  virtual void start() = 0;
};

}  // namespace rtc

#endif