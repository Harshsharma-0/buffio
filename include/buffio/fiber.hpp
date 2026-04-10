/*
 * CAUTION: don't mess with the include order of this file.
 */

#pragma once

#include "buffio/Queue.hpp"
#include "buffio/actions.hpp"
#include "buffio/enum.hpp"
#include "buffio/clock.hpp"
#include "buffio/common.hpp"
#include "buffio/memory.hpp"
#include "buffio/sockbroker.hpp"
#include <atomic>

namespace buffio {

namespace fiber {

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
  };
  clamper() {
    if (info.header == nullptr)
      return;
    info.header->action = buffio::action::clampThread;
  };
  clampInfo sclamp() const { return info; };
  clampInfo sclamp(auto then) {
    return info;
  }

  void clamp(auto then) {
    buffio::fiber::threadRequestBatch->push(info.header);
  }
  void clamp() { 
    buffio::fiber::threadRequestBatch->push(info.header);
  }

private:
  clampInfo info = {new buffioHeader};
};

}; // namespace fiber
}; // namespace buffio

#include "buffio/promise.hpp"
