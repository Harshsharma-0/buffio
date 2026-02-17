#include "buffio/clock.hpp"
#include "buffio/promise.hpp"
#include <chrono>

namespace buffio {

int Clock::getNext() {
  if (clockTree.empty())
    return -1;

  auto now = chrClock::now();
  auto next = clockTree.top().expires;

  if (next <= now)
    return 0;
  auto diff =
      std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count();
  return static_cast<int>(diff);
};

void Clock::push(uint32_t delay, std::coroutine_handle<> task) {

  assert(this != nullptr);
  clockTree.push({std::chrono::milliseconds(delay) + chrClock::now(), task});
};

void Clock::pushExpired(buffio::Queue<> &queue) {
  if (clockTree.empty())
    return;

  auto now = chrClock::now();
  auto next = clockTree.top().expires;
  auto diff =
      std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count();

  while (diff <= 0) {
    auto handle = clockTree.top().task;
    auto *promise = getPromise<char>(handle);
    queue.push(handle);
    clockTree.pop();
    if (clockTree.empty())
      break;
    next = clockTree.top().expires;
    diff = std::chrono::duration_cast<std::chrono::milliseconds>(next - now)
               .count();
  }
};
}; // namespace buffio
