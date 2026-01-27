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

#include "buffioenum.hpp"
#if !defined(BUFFIO_IMPLEMENTATION)
// #include "buffioenum.hpp"
#include "buffiofd.hpp"
#include "buffiolfqueue.hpp"
#include "buffiopromise.hpp"
#include "buffiothread.hpp"
#include <memory>
#endif

#include <sys/epoll.h>

using buffioSockBrokerQueue = buffiolfqueue<buffioFdView>;

class buffioSockBroker {

  // the main thread worker code inlined with the code to support hybrid arch
  // only call to register fd for read write.

  __attribute__((used)) static int buffioWorker(void *data) { return 0; }
  __attribute__((used)) static int buffioEpollPoller(void *data) {

    struct epoll_event evnt[1024];

    return 0;
  }

public:
  buffioSockBroker() : epollFd(-1), epollTotalFd(0), ioUringFd(-1) {

    sockBrokerState = buffioSockBrokerState::inActive;
  };

  ~buffioSockBroker() {
    switch (sockBrokerState) {
    case buffioSockBrokerState::epollRunning:
    case buffioSockBrokerState::epoll:
      shutpoll();
      break;
    }
    return;
  }

  int start(buffioThreadView &thread, int workerNum = 4, int queueOrder = 5) {

    size_t queueSizeRel = 1 << queueOrder;

    if (workerNum > queueSizeRel)
      return (int)buffioErrorCode::workerNum;
    if (queueOrder < BUFFIO_RING_MIN || queueOrder > buffioatomix_max_order)
      return (int)buffioErrorCode::queueSize;
    if (epollEntry.lfstart(queueSize, nullptr) < 0)
      return (int)buffioErrorCode::epollEntry;

    if (epollConsume.lfstart(queueSize, nullptr) < 0)
      goto outWithCleanUp;

    if (epollWorks.lfstart(queueSize, nullptr) < 0)
      goto outWithCleanUp;

    if ((epollFd = ::epoll_create1(EPOLL_CLOEXEC)) < 0)
      goto outWithCleanUp;

    return (int)buffioErrorCode::none;

  outWithEpoll:
    ::close(epollFd);
  outWithCleanUp:
    epollEntry.~buffiolfqueue();
    epollConsume.~buffiolfqueue();
    epollWorks.~buffiolfqueue();
    return (int)buffioErrorCode::none;
  }

  int pushreq() const { return 0; }
  int popreq() const { return 0; }

  bool running() const {
    return (sockBrokerState != buffioSockBrokerState::inActive &&
            sockBrokerState != buffioSockBrokerState::error);
  };
  int pollOp(int opCode) {
    if (!running())
      return (int)buffioErrorCode::epollInstance;
    return (int)buffioErrorCode::none;
  };
  buffioSockBroker(buffioSockBroker const &) = delete;
  buffioSockBroker &operator=(buffioSockBroker const &) = delete;
  buffioSockBroker(buffioSockBroker const &&) = delete;
  buffioSockBroker &operator=(buffioSockBroker const &&) = delete;

private:
  int setupIoUring() { return 0; }

  buffioThreadView threadPool;
  buffioSockBrokerQueue epollEntry;
  buffioSockBrokerQueue epollWorks;
  buffioSockBrokerQueue epollConsume;
  size_t epollTotalFd;
  std::atomic<int> epollEmpty;
  buffioSockBrokerState sockBrokerState;
  int epollFd;
  int ioUringFd;
  int workerNum; // number of the workers;
  int queueSize;
};

#endif
