#include "buffio/clock.hpp"

namespace buffio {

int Clock::getNext(uint64_t looptime) {
  if (count == 0)
    return -1;

  auto timerOf = timerTree.top();
  uint64_t offset = timerOf.expires < looptime ? 0 : timerOf.expires - looptime;
  if (offset == 0) {
    timerTree.pop();
    count -= 1;
  };
  nextWork = timerOf.task;
  return static_cast<int>(offset);
};

void Clock::push(uint64_t ms, std::coroutine_handle<> task) {

  assert(this != nullptr);
  this->count += 1;
  timerTree.push({ms + now(), task});
};

uint64_t Clock::now() noexcept {
  struct timespec tv;
  if (clock_gettime(CLOCK_MONOTONIC, &tv) != 0)
    return -1;
  return (uint64_t)tv.tv_sec * 1000 + tv.tv_nsec / 1000000;
};
}; // namespace buffio
