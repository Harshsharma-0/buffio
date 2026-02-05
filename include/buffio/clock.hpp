#pragma once

#include <cassert>
#include <coroutine>
#include <cstdint>
#include <queue>
#include <time.h>

struct buffioTimerInfo {
  uint64_t expires;
  std::coroutine_handle<> task;
};

namespace buffio {

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
  Clock() : count(0) {};
  ~Clock() = default;

  int getNext(uint64_t looptime);
  bool empty() const { return (count == 0); }

  uint64_t now() noexcept;

  inline std::coroutine_handle<> get() const { return nextWork; }
  inline void push(uint64_t ms, std::coroutine_handle<> task);

private:
  size_t count;
  buffioClockTree timerTree;
  std::coroutine_handle<> nextWork;
};
}; // namespace buffio
