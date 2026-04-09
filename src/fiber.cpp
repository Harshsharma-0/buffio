#include "buffio/fiber.hpp"


namespace buffio {
namespace fiber {


buffio::Queue<buffioHeader, void *, buffioQueueNoMem> *requestBatch = nullptr;
buffio::Queue<buffioHeader, void *, buffioQueueNoMem> *threadRequestBatch = nullptr;
buffio::Queue<> *queue = nullptr;
buffio::Clock *timerClock = nullptr;
buffio::sockBroker *poller = nullptr;
std::atomic<size_t> workerCount = 0;
std::atomic<ssize_t> abort = 0;
std::atomic<ssize_t> FdCount = 0;
std::atomic<ssize_t> pendingReq = 0;
std::atomic<ssize_t> sleepingThread = 0;
std::atomic<ssize_t> queuedCompleted = 0;
std::atomic <bool> loopWakedUp = false;

}; // namespace fiber
}; // namespace buffio
