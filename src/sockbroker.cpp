

#include "buffio/sockbroker.hpp"
#include "buffio/fd.hpp"
#include "buffio/fiber.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>

namespace buffio {
int sockBroker::worker(void *data) {
  buffio::sockBroker *parent = (buffio::sockBroker *)data;
  buffioSockBrokerQueue *workQueue = &parent->epollWorks;
  buffioSockBrokerQueue *consumeQueue = &parent->epollConsume;
  bool exit = false;
  ssize_t abort = 0;

  buffio::fiber::workerCount.fetch_add(1, std::memory_order_acq_rel);
  while (exit != true) {
    abort = buffio::fiber::abort.load(std::memory_order_acquire);
    if (abort < 0)
      break;
    if (workQueue->empty()) {
      ::sem_wait(&parent->buffioWorkerSignal);
      abort = buffio::fiber::abort.load(std::memory_order_acquire);
      if (abort < 0) {
        break;
      }
    };

    buffioHeader *tmpWork = workQueue->dequeue(nullptr);
    if (tmpWork == nullptr)
      continue;

    tmpWork->routine = tmpWork->action(tmpWork);

    consumeQueue->enqueue(tmpWork);
    parent->wakeLoop();

    buffio::fiber::pendingReq.fetch_add(-1, std::memory_order_acq_rel);
  };
  buffio::fiber::workerCount.fetch_add(-1, std::memory_order_acq_rel);

  return 0;
};

int sockBroker::start(buffio::thread &thread, int &workerNum,
                      size_t queueOrder) {

  size_t queueSizeRel = 1 << queueOrder;
  if (workerNum > queueSizeRel)
    return (int)buffioErrorCode::workerNum;
  if (queueOrder < BUFFIO_RING_MIN || queueOrder > buffioatomix_max_order)
    return (int)buffioErrorCode::queueSize;

  if (::sem_init(&buffioWorkerSignal, 0, 0) != 0)
    return -1;

  if (epollConsume.lfstart(queueOrder) < 0)
    goto outWithCleanUp;

  if (epollWorks.lfstart(queueOrder) < 0)
    goto outWithCleanUp;

  if ((epollFd = ::epoll_create1(EPOLL_CLOEXEC)) < 0)
    goto outWithCleanUp;

  for (int i = 0; i < workerNum; i++) {
    if (thread.run(nullptr, buffio::sockBroker::worker, this) != 0) {
      workerNum = i + 1;
      return (int)buffioErrorCode::threadRun;
    }
  };
  sockBrokerState = buffioSockBrokerState::active;

  return (int)buffioErrorCode::none;

outWithEpoll:
  ::close(epollFd);
outWithCleanUp:
  epollConsume.~lfqueue();
  epollWorks.~lfqueue();
  return (int)buffioErrorCode::none;
};
}; // namespace buffio
