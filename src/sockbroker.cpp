#include "buffio/sockbroker.hpp"
#include "buffio/fiber.hpp"
#include <atomic>
#include <cstdint>
#include <iostream>

namespace buffio {
int sockBroker::worker(void *data) {
  buffio::sockBroker *parent = (buffio::sockBroker *)data;
  buffioSockBrokerQueue *workQueue = &parent->epollWorks;
  buffioSockBrokerQueue *consumeQueue = &parent->epollConsume;
  bool exit = false;

  buffio::fiber::workerCount.fetch_add(1, std::memory_order_acq_rel);

  while (exit != true) {
    ::sem_wait(&parent->buffioWorkerSignal);
    buffioHeader *tmpWork = workQueue->dequeue(nullptr);
    if (tmpWork == nullptr)
      break;
    if (tmpWork->opCode == buffioOpCode::abort) {
      exit = true;
      break;
    };
    int error = 0;
    switch (tmpWork->opCode) {
    case buffioOpCode::asyncRead:
    case buffioOpCode::asyncWrite:
    case buffioOpCode::read:
    case buffioOpCode::write: {
      while (error != -1 || error != 0) {
        error = buffio::sockBroker::consumeEntry(tmpWork);
      };
      tmpWork->opError = error;
      consumeQueue->enqueue(tmpWork);
    } break;
    case buffioOpCode::asyncReadFile:
      [[fallthrough]];
    case buffioOpCode::readFile: {
      error =
          ::read(tmpWork->reqToken.fd, tmpWork->data.buffer, tmpWork->len.len);
      tmpWork->opError = error;

    } break;
    case buffioOpCode::asyncWriteFile:
      [[fallthrough]];
    case buffioOpCode::writeFile: {
      error =
          ::write(tmpWork->reqToken.fd, tmpWork->data.buffer, tmpWork->len.len);
      tmpWork->opError = error;

    } break;
    default:
      auto handle = buffio::sockBroker::handleAsync(tmpWork);
      if (!handle) {
        tmpWork->opError = -1;
      } else {
        tmpWork->routine = handle;
      };
      break;
    };
    consumeQueue->enqueue(tmpWork);
  };
  buffio::fiber::workerCount.fetch_add(-1, std::memory_order_acq_rel);

  return 0;
};

buffio::promiseHandle sockBroker::handleAsync(buffioHeader *req) {

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

int sockBroker::consumeEntry(buffioHeader *req) {

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
        *req->fd | req->unsetBit; // set bit's read/write ready it the fd was
                                  // just picked from the queue.
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
  epollConsume.~buffiolfqueue();
  epollWorks.~buffiolfqueue();
  return (int)buffioErrorCode::none;
};
}; // namespace buffio
