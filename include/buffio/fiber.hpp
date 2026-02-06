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
}; // namespace fiber
}; // namespace buffio
