#pragma once

#include "common.hpp"
#include "enum.hpp"
#include "fd.hpp"
#include "lfqueue.hpp"
#include "promise.hpp"
#include "thread.hpp"
#include <semaphore.h>
#include <sys/epoll.h>
#include <unistd.h>

#define BUFFIO_REQUEST_MAX_SIZE sizeof(buffioRequestMaxSize)
using buffioSockBrokerQueue = buffiolfqueue<buffioHeader *>;

namespace buffio {
class sockBroker {
public:
  // the main thread worker code inlined with the code to
  // support hybrid arch
  // only call to register fd for read write.

  static int worker(void *data);
  sockBroker(sockBroker const &) = delete;
  sockBroker &operator=(sockBroker const &) = delete;
  sockBroker(sockBroker const &&) = delete;
  sockBroker &operator=(sockBroker const &&) = delete;
  sockBroker() : epollFd(-1), count(0) {
    sockBrokerState = buffioSockBrokerState::none;
  };

  ~sockBroker() {}

  static buffioPromiseHandle handleAsync(buffioHeader *req);
  /**
   * @brief consumeEntry is used to process the events received from the epoll
   * and that are not edge-triggered.
   *
   * @param [in] req  pointer to the request Header that need to be processed.
   *
   * @return 0 on an entry is done,
   * @returns 1 if the entry is not fully consumed;
   * @returns -1 if there error occured and errno is set.
   *
   */

  static int consumeEntry(buffioHeader *req);
  int start(buffio::thread &thread, int workerNum = 2, size_t queueOrder = 11);

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

  int pollOp(int fd, void *data, int32_t mask = EPOLLIN | EPOLLOUT) {
    if (!running())
      return (int)buffioErrorCode::epollInstance;

    struct epoll_event evnt = {0};
    evnt.events |= mask;
    evnt.data.ptr = data;

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

}; // namespace buffio
