#ifndef BUFFIO_SOCK_BROKER
#define BUFFIO_SOCK_BROKER
/*
 * Error codes range reserved for buffiosockbroker
 *
 *  [1000 - 2000]
 *  1000 <= errorcode <= 2000
 *
 *
 *
 */

#if !defined(BUFFIO_IMPLEMENTATION)
// #include "buffioenum.hpp"
#include "buffiofd.hpp"
#include "buffiolfqueue.hpp"
#include "buffiopromise.hpp"
#include "buffiothread.hpp"

#endif

#include <semaphore.h>
#include <sys/epoll.h>

#define BUFFIO_REQUEST_MAX_SIZE sizeof(buffioRequestMaxSize)
using buffioSockBrokerQueue = buffiolfqueue<buffioHeader *>;

class buffioSockBroker {

  // the main thread worker code inlined with the code to
  // support hybrid arch
  // only call to register fd for read write.

  __attribute__((used)) static int buffioWorker(void *data) {
    buffioSockBroker *parent = (buffioSockBroker *)data;
    buffioSockBrokerQueue *workQueue = &parent->epollWorks;
    buffioSockBrokerQueue *consumeQueue = &parent->epollConsume;
    int err = 0;
    int count = 0;
    bool exit = false;
    buffioHeader *work[8];

    while (exit != true) {
      ::sem_wait(&parent->buffioWorkerSignal);
      for (int i = 0; i < 4; i++) {
        buffioHeader *tmpWork = workQueue->dequeue(nullptr);
        if (tmpWork == nullptr)
          break;
        if (tmpWork->opCode == buffioOpCode::abort) {
          exit = true;
          continue;
        }
        work[i] = tmpWork;
        count += 1;
      };

      if (count == 0)
        continue;

      for (int j = 0; j < count; j++) {
        buffioHeader *header = work[j];
        buffioOpCode opCode = header->opCode;
        switch (opCode) {
        case buffioOpCode::read:
          break;
        case buffioOpCode::write:
          break;
        default:
          break;
        };
      };

      count = 0; // reset count
    }
    return 0;
  };

public:
  buffioSockBroker(buffioSockBroker const &) = delete;
  buffioSockBroker &operator=(buffioSockBroker const &) = delete;
  buffioSockBroker(buffioSockBroker const &&) = delete;
  buffioSockBroker &operator=(buffioSockBroker const &&) = delete;
  buffioSockBroker() : epollFd(-1), count(0) {
    sockBrokerState = buffioSockBrokerState::none;
  };
  ~buffioSockBroker() {}

  int start(buffioThread &thread, int workerNum = 2, size_t queueOrder = 11) {

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

    sockBrokerState = buffioSockBrokerState::active;
    thread.run("buffio-", buffioSockBroker::buffioWorker, this);
    return (int)buffioErrorCode::none;

  outWithEpoll:
    ::close(epollFd);
  outWithCleanUp:
    epollConsume.~buffiolfqueue();
    epollWorks.~buffiolfqueue();
    return (int)buffioErrorCode::none;
  };

  inline int pushreq(buffioHeaderType *which) {
    if (sockBrokerState == buffioSockBrokerState::active) {
      count += 1;
      return epollWorks.enqueue(which);
    }
    return -1;
  }
  inline int push(buffioHeaderType *which) {
    if (sockBrokerState == buffioSockBrokerState::active) {
      count += 1;
      return epollWorks.enqueue(which);
    }
    return -1;
  }
  inline buffioHeaderType *pop() {
    if (sockBrokerState == buffioSockBrokerState::active) {
      count -= 1;
      return epollWorks.dequeue(nullptr);
    }

    return nullptr;
  }

  inline buffioHeaderType *popreq() {
    if (sockBrokerState == buffioSockBrokerState::active) {
      count -= 1;
      return epollWorks.dequeue(nullptr);
    }

    return nullptr;
  }

  bool running() const {
    return (sockBrokerState == buffioSockBrokerState::active);
  };
  bool busy() const { return (count != 0); }

  int pollOp(int fd, void *data, uint32_t opCode = 0) {
    if (!running())
      return (int)buffioErrorCode::epollInstance;

    struct epoll_event evnt;
    evnt.events = (EPOLLIN | EPOLLOUT);
    evnt.data.ptr = data;
    // TODO: check ERROR
    //
    int retcode = epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &evnt);
    return (int)buffioErrorCode::none;
  };

  int pollDel(int fd) {
    if (!running())
      return (int)buffioErrorCode::epollInstance;

    // TODO: check ERROR
    int retcode = epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL);
    return (int)buffioErrorCode::none;
  };

  size_t poll(struct epoll_event *event, size_t size, uint64_t timeout) const {
    if (!running())
      return (int)buffioErrorCode::epollInstance;

    return epoll_wait(epollFd, event, size, timeout);
  };

  int ping() {
    if (!running())
      return (int)buffioErrorCode::epollInstance;
    ::sem_post(&buffioWorkerSignal);
    return 0;
  }

private:
  buffioSockBrokerQueue epollWorks;
  buffioSockBrokerQueue epollConsume;
  buffioSockBrokerState sockBrokerState;
  int epollFd;
  size_t count;
  sem_t buffioWorkerSignal;
};

#endif
