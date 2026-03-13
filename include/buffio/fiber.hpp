/*
 * CAUTION: don't mess with the include order of this file.
 */

#pragma once

#include "Queue.hpp"
#include "clock.hpp"
#include "common.hpp"
#include "memory.hpp"
#include "sockbroker.hpp"
#include <atomic>

namespace buffio {

namespace fiber {

extern int value;
extern buffio::Queue<buffioHeader, void *, buffioQueueNoMem> *requestBatch;
extern buffio::Memory<buffioHeader> *headerPool;
extern buffio::Queue<> *queue;
extern buffio::Clock *timerClock;
extern buffio::sockBroker *poller;
extern std::atomic<size_t> workerCount;
extern std::atomic<ssize_t> abort; // below 0 to abort,
extern std::atomic<ssize_t> FdCount;
extern std::atomic<ssize_t> pendingReq;

typedef struct {
  buffioHeader *header;
  bool isSelf;
} clampInfo;

class clamper {
public:
  clamper() {
    if (info.header == nullptr)
      return;
    error = 0;
    info.header->opCode = buffioOpCode::clampThread;
  };

  clamper(auto handle){
    if (info.header == nullptr)
        return;
      
    error = 0;
    info.header->opCode = buffioOpCode::clampThread;
    info.header->routine = handle.get();
  };

  clampInfo clamp()const { return info;};

private:
  int error = -1;
  clampInfo info = {buffio::fiber::headerPool->pop(),false};
};

}; // namespace fiber
}; // namespace buffio

#include "promise.hpp"
