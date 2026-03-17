#include "buffio/fiber.hpp"

namespace buffio {

namespace fiber {

int value = -110;
buffio::Queue<buffioHeader, void *, buffioQueueNoMem> *requestBatch = nullptr;
buffio::Queue<> *queue = nullptr;
buffio::Clock *timerClock = nullptr;
buffio::sockBroker *poller = nullptr;
std::atomic<size_t> workerCount = 0;
std::atomic<ssize_t> abort = 0;
std::atomic<ssize_t> FdCount = 0;
std::atomic<ssize_t> pendingReq = 0;

void clamper::confSelf(){
 auto promise = getPromise<char>(info.header->routine);
  promise->setAux((uintptr_t)info.header,true);
};
}; // namespace fiber
}; // namespace buffio
