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
#include <unistd.h>
#if !defined(BUFFIO_IMPLEMENTATION)
// #include "buffioenum.hpp"
#include "buffiofd.hpp"
#include "buffiolfqueue.hpp"
#include "buffiopromise.hpp"
#include "buffiothread.hpp"

#endif

#include "buffiocommon.hpp"
#include "./buffiopromise.hpp"

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
    bool exit = false;

    while (exit != true) {
      ::sem_wait(&parent->buffioWorkerSignal);
      buffioHeader *tmpWork = workQueue->dequeue(nullptr);
      if (tmpWork == nullptr)
        break;
      if (tmpWork->opCode == buffioOpCode::abort) {
        exit = true;
        continue;
      };
      int error = 0;
      switch (tmpWork->opCode) {
      case buffioOpCode::asyncRead:
      case buffioOpCode::asyncWrite:
      case buffioOpCode::read:
      case buffioOpCode::write: {
        while (error != -1 || error != 0) {
          error = buffioSockBroker::consumeEntry(tmpWork);
        };
        tmpWork->opError = error;
        consumeQueue->enqueue(tmpWork);
      } break;
      case buffioOpCode::asyncReadFile:
        [[fallthrough]];
      case buffioOpCode::readFile: {
        error = ::read(tmpWork->reqToken.fd, tmpWork->data.buffer,
                       tmpWork->len.len);
        tmpWork->opError = error;

      } break;
      case buffioOpCode::asyncWriteFile:
        [[fallthrough]];
      case buffioOpCode::writeFile: {
        error = ::write(tmpWork->reqToken.fd, tmpWork->data.buffer,
                        tmpWork->len.len);
        tmpWork->opError = error;

      } break;
      default:
        auto  handle = buffioSockBroker::handleAsync(tmpWork);
        if(!handle){
            tmpWork->opError = -1;
          }else{
            tmpWork->routine = handle;
          };
          break;
      };
      consumeQueue->enqueue(tmpWork);
    };
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

  static buffioPromiseHandle handleAsync(buffioHeader *req) {
    switch (req->opCode) {
    case buffioOpCode::asyncConnect: {
      int code = -1;
      socklen_t len = sizeof(int);
      if (::getsockopt(req->reqToken.fd, SOL_SOCKET, SO_ERROR, &code, &len) !=
          0) {
        auto handle =
            req->onAsyncDone.onAsyncConnect(-1, nullptr, req->data.socketaddr)
                .get();
        return handle;
        break;
      };
      auto handle =
          req->onAsyncDone.onAsyncConnect(code, req->fd, req->data.socketaddr)
              .get();
      return handle;
    } break;
    case buffioOpCode::asyncAcceptlocal: {
      sockaddr_un addr = {0};
      socklen_t len = sizeof(addr);
      int fd = ::accept(req->reqToken.fd, (sockaddr *)&addr, &len);
      auto handle = req->onAsyncDone.asyncAcceptlocal(fd, addr, len).get();
      return handle;
    } break;
    case buffioOpCode::asyncAcceptin: {
      sockaddr_in addrin = {0};
      socklen_t lenin = sizeof(addrin);
      int fd = ::accept(req->reqToken.fd, (sockaddr *)&addrin, &lenin);
      auto handle = req->onAsyncDone.asyncAcceptin(fd, addrin, lenin).get();
      return handle;
    }; break;
    case buffioOpCode::asyncAcceptin6: {
      sockaddr_in6 addr6 = {0};
      socklen_t len6 = sizeof(addr6);
      int fd = ::accept(req->reqToken.fd, (sockaddr *)&addr6, &len6);
      auto handle = req->onAsyncDone.asyncAcceptin6(fd, addr6, len6).get();
      return handle;
      break;
    };
    case buffioOpCode::waitAccept:
      [[fallthrough]];
    case buffioOpCode::waitConnect:
      return req->routine;
      break;
    };

    return nullptr;
  };

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

  static int consumeEntry(buffioHeader *req) {
    // consumeBatch only handle read and write requests;
    ssize_t buffiolen = -1;

    switch (req->rwtype) {
    case buffioReadWriteType::read:
      buffiolen = ::read(req->reqToken.fd, req->bufferCursor, req->reserved);
      [[fallthrough]];
    case buffioReadWriteType::write:
      buffiolen = ::write(req->reqToken.fd, req->bufferCursor, req->reserved);
      [[fallthrough]];
    case buffioReadWriteType::rwEnd:
      if (buffiolen > 0) {
        req->bufferCursor += buffiolen; // moving the buffer to the next bytes;
        req->reserved -= buffiolen;
        if (req->reserved <= 0) {
          req->len.len = (req->bufferCursor - req->data.buffer);
          return 0;
        };
      };
      break;
    case buffioReadWriteType::recvfrom:
      [[fallthrough]];
    case buffioReadWriteType::recv:
      break;
    case buffioReadWriteType::sendto:
      [[fallthrough]];
    case buffioReadWriteType::send:
      break;
    default:
      break;
    };

    if (buffiolen < 0)
      return -1;
    return 1;
  };

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

#endif
