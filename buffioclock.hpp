#ifndef BUFFIO_CLOCK
#define BUFFIO_CLOCK

#include <cstdint>
#include <queue>
#include <time.h>


struct buffioTimerInfo {
  uint64_t expires;
  std::coroutine_handle<> task;
};

class buffioClock {

  struct buffioTimerCmp {
    bool operator()(const buffioTimerInfo &a, const buffioTimerInfo &b) const {
      return a.expires > b.expires;
    };
  };
  using buffioClockTree =
      std::priority_queue<buffioTimerInfo, std::vector<buffioTimerInfo>,
                          buffioTimerCmp>;

public:
  buffioClock() : count(0) {};
  ~buffioClock() = default;

  int getNext(uint64_t looptime) {
    if (count == 0)
      return -1;

    auto timerOf = timerTree.top();
    uint64_t offset =
        timerOf.expires < looptime ? 0 : timerOf.expires - looptime;
    if (offset == 0) {
      timerTree.pop();
        count -= 1;
    };
    nextWork = timerOf.task;
    return static_cast<int>(offset);
  };

  inline std::coroutine_handle<> get() const { return nextWork; }
  inline void push(uint64_t ms, std::coroutine_handle<> task) {
    assert(this != nullptr);
    this->count += 1;
    timerTree.push({ms + now(),task});
  };

  bool empty() const { return (count == 0); }

  uint64_t now() noexcept {
    struct timespec tv;
    if (clock_gettime(CLOCK_MONOTONIC, &tv) != 0)
      return -1;
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_nsec / 1000000;
  };

private:
  buffioClockTree timerTree;
  std::coroutine_handle<> nextWork;
  size_t count;
};
#endif
