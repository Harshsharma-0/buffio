#pragma once

#include "Queue.hpp"
#include <cassert>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <queue>
#include <ratio>
#include <time.h>

using chrClock = std::chrono::steady_clock;
struct buffioTimerInfo {
  chrClock::time_point expires;
  std::coroutine_handle<> task;
};

namespace buffio {

namespace clockSpec {
struct wait {
  uint32_t ms;
};
}; // namespace clockSpec
struct buffioTimerCmp {
  bool operator()(const buffioTimerInfo &a, const buffioTimerInfo &b) const {
    return a.expires > b.expires;
  };
};
using buffioClockTree =
    std::priority_queue<buffioTimerInfo, std::vector<buffioTimerInfo>,
                        buffioTimerCmp>;
class Clock {
public:
  Clock() = default;
  ~Clock() = default;

  int getNext();
  bool empty() const { return clockTree.empty(); }

  void push(uint32_t delay, std::coroutine_handle<> task);
  void pushExpired(buffio::Queue<> &queue);

private:
  buffioClockTree clockTree;
  std::coroutine_handle<> nextWork;
};
}; // namespace buffio
