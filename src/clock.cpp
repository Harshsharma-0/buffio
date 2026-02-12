#include "buffio/clock.hpp"
#include "buffio/promise.hpp"
#include <chrono>

namespace buffio {

int Clock::getNext() {
  if (timerTree.empty())
    return -1;

  auto now = chrClock::now();
  auto next = timerTree.top().expires;

  if (next <= now)
    return 0;
  auto diff =
      std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count();
  return static_cast<int>(diff);
};

void Clock::push(uint32_t delay, std::coroutine_handle<> task) {

  assert(this != nullptr);
  timerTree.push({std::chrono::milliseconds(delay) + chrClock::now(), task});
};

void Clock::pushExpired(buffio::Queue<> &queue) {
  if (timerTree.empty())
    return;

  auto now = chrClock::now();

  while (!timerTree.empty() && timerTree.top().expires <= now) {
    auto handle = timerTree.top().task;
    auto *promise = getPromise<char>(handle);
    promise->setStatus(buffioRoutineStatus::executing);
    queue.push(handle);
    timerTree.pop();
  }
};
}; // namespace buffio
