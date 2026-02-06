#include "buffio/fiber.hpp"
#include <iostream>

namespace buffio {

namespace fiber {

int value = -110;
buffio::Queue<buffioHeader, void *, buffioQueueNoMem> *requestBatch = nullptr;
buffio::Memory<buffioHeader> *headerPool = nullptr;
buffio::Queue<> *queue = nullptr;
buffio::Clock *timerClock = nullptr;
buffio::sockBroker *poller = nullptr;
std::atomic<size_t> workerCount = 0;
}; // namespace fiber
}; // namespace buffio
