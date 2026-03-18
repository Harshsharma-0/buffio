/*
 * CAUTION: don't mess with the include order of this file.
 */

#pragma once

#include "Queue.hpp"
#include "buffio/actions.hpp"
#include "buffio/enum.hpp"
#include "clock.hpp"
#include "common.hpp"
#include "memory.hpp"
#include "sockbroker.hpp"
#include <atomic>

namespace buffio {

namespace fiber {

extern int value;
extern buffio::Queue<buffioHeader, void *, buffioQueueNoMem> *requestBatch;
extern buffio::Queue<buffioHeader, void *, buffioQueueNoMem> *threadRequestBatch;
extern buffio::Queue<> *queue;
extern buffio::Clock *timerClock;
extern buffio::sockBroker *poller;

extern std::atomic<size_t> workerCount;
extern std::atomic<ssize_t> abort; // below 0 to abort,
extern std::atomic<ssize_t> FdCount;
extern std::atomic<ssize_t> pendingReq;
extern std::atomic<ssize_t> sleepingThread;

extern std::atomic<ssize_t> queuedCompleted;

extern std::atomic<bool> loopWakedUp;

typedef struct {
  buffioHeader *header;
} clampInfo;

typedef struct clampNs {
  buffioHeader *header;
  buffio::promiseHandle then;
} clampNs;
class clamper {
public:
  clamper(auto then) {
    if (info.header == nullptr)
      return;
    info.header->action = buffio::action::clampThread;
    info.header->routine = then.get();
    confSelf();
  };
  clamper() {
    if (info.header == nullptr)
      return;
    info.header->action = buffio::action::clampThread;
    info.header->routine = nullptr;
  };
  clampInfo sclamp() const { return info; };
  clampInfo sclamp(auto then) {
    info.header->routine = then.get();
    return info;
  }

  void clamp(auto then) {
    info.header->then = then.get();
    buffio::fiber::threadRequestBatch->push(info.header);
  }
  void clamp() { 
    info.header->then = nullptr;
    buffio::fiber::threadRequestBatch->push(info.header);
  }

private:
  void confSelf();
  clampInfo info = {new buffioHeader};
};

}; // namespace fiber
}; // namespace buffio

#include "buffio/promise.hpp"
