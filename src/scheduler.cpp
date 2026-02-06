#include "buffio/scheduler.hpp"
#include <iostream>
#include <unistd.h>

namespace buffio {

scheduler::scheduler() {
  // setting up the fiber
  buffio::fiber::poller = &this->poller;
  buffio::fiber::queue = &this->queue;
  buffio::fiber::timerClock = &this->timerClock;
  buffio::fiber::headerPool = &this->headerPool;
  buffio::fiber::requestBatch = &this->requestBatch;
};
scheduler::~scheduler() {
  buffio::fiber::poller = nullptr;
  buffio::fiber::queue = nullptr;
  buffio::fiber::timerClock = nullptr;
  buffio::fiber::headerPool = nullptr;
  buffio::fiber::requestBatch = nullptr;
};

int scheduler::run() {
  if (queue.empty() && timerClock.empty())
    return -1;

  int workerNum = 2;
  int error = 0;
  if ((error = headerPool.init()) != 0)
    return error;
  if ((error = poller.start(threadPool, workerNum)) != 0)
    return error;
  if ((error = buffio::MakeFd::pipe(syncPipe)) != 0)
    return error;
  if ((error = poller.pollOp(syncPipe.getPipeRead(), &syncPipe)) != 0) {
    syncPipe.release();
    return error;
  };

  struct epoll_event evnt[1024];

  /*
   * batch of fd allocated to the main event loop to process, is just a
   * circular list, as we support EPOLLET edge-triggered mode, then we need
   * batch procssing.
   */
  bool exit = false;
  int timeout = 0;

  while (exit != true) {

    timeout = getWakeTime(&exit);

    if (exit == true)
      break;

    int nfd = poller.poll(evnt, 1024, timeout);

    if (nfd < 0)
      break;
    if (nfd != 0) {
      processEvents(evnt, nfd);
    }

    yieldQueue(10); // yielding queue before batch consumption;
    consumeBatch(8);
    yieldQueue(10); // yielding queue after batch consumption;
    getWakeTime(&exit);
  };

  cleanQueue();
  shutWorker(workerNum, 10, 200);
  threadPool.free();
  return 0;
};

void scheduler::shutWorker(int workerNum, int tries, long wait) {
  for (int i = 0; i < workerNum; i++) {
    auto header = headerPool.pop();
    header->opCode = buffioOpCode::abort;
    poller.push(header);
    poller.ping();
  };

  size_t workerCount =
      buffio::fiber::workerCount.load(std::memory_order_acquire);

  struct timespec ts;
  ts.tv_sec = wait / 1000;
  ts.tv_nsec = (wait % 1000) * 100000L;

  for (size_t i = workerCount; i > 0;) {
    ::nanosleep(&ts, &ts);
    i = buffio::fiber::workerCount.load(std::memory_order_acquire);
    tries -= 1;
    if (tries < 0)
      break;
  };
};
int scheduler::processEvents(struct epoll_event evnts[], int len) {
  for (int i = 0; i < len; i++) {
    auto handle = (buffio::Fd *)evnts[i].data.ptr;
    if (evnts[i].events & EPOLLIN) {
      auto req = handle->getPendingRead();
      if (req != nullptr) {
        handle->popPendingRead();
        requestBatch.push(req);
      } else {
        *handle | BUFFIO_READ_READY;
      }
    }

    if (evnts[i].events & EPOLLOUT) {
      auto req = handle->getPendingWrite();
      if (req != nullptr) {
        handle->popPendingWrite();
        requestBatch.push(req);
      } else {
        *handle | BUFFIO_WRITE_READY;
      }
    };

    auto req = handle->getReserveHeader();

    switch (req->opCode) {
    case buffioOpCode::none:
      continue;
      break;
    case buffioOpCode::read:
    case buffioOpCode::write:
      requestBatch.push((buffioHeader *)req);
      break;
    };

    auto routine = buffio::sockBroker::handleAsync((buffioHeader *)req);
    if (routine) {
      queue.push(routine);
    }
  };
  // calls like accept and connect are handled here;
  return 0;
};

int scheduler::dispatchHandle(int errorCode, buffio::Fd *fd,
                              buffioHeader *header) {
  switch (header->opCode) {
  case buffioOpCode::read:
    [[fallthrough]];
  case buffioOpCode::readFile:
    queue.push(header->routine);
    break;
  case buffioOpCode::write:
    [[fallthrough]];
  case buffioOpCode::writeFile:
    queue.push(header->routine);
    break;
  case buffioOpCode::asyncRead: {
    auto handle = header->onAsyncDone
                      .onAsyncWrite(errorCode, header->data.buffer,
                                    header->len.len, fd, header)
                      .get();
    queue.push(handle);
  } break;
  case buffioOpCode::asyncWrite: {
    auto handle = header->onAsyncDone
                      .onAsyncRead(errorCode, header->data.buffer,
                                   header->len.len, fd, header)
                      .get();
    queue.push(handle);
  } break;
  default:
    assert(false);
    break;
  };
  header->opCode = buffioOpCode::done;
  return 0;
};

int scheduler::consumeBatch(int cycle) {
  if (requestBatch.empty())
    return -1;
  // consumeBatch only handle read and write requests;
  ssize_t buffiolen = -1;

  for (int i = 0; i < cycle; i++) {
    auto req = requestBatch.get();
    int error = buffio::sockBroker::consumeEntry(req);
    if (error == 1)
      goto continueloop;
    if (error <= 0) {
      if (error == 0)
        dispatchHandle(-1, req->fd, req);
      requestBatch.pop();
      goto continueloop;
    };

    // returned when there no data to read or write and we have consumed the
    // batch && bufferlen = -1
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      req->fd->unsetBit(req->unsetBit);
      requestBatch.pop();
      dispatchHandle(0, req->fd, req);
      goto continueloop;
    } else {
      dispatchHandle(errno, req->fd, req);
    };

  continueloop:
    if (requestBatch.empty())
      break;
    requestBatch.mvNext();
  };
  return 0;
};

void scheduler::cleanQueue() {
  while (!queue.empty()) {
    auto handle = queue.get();
    if (handle->waiter)
      handle->waiter->current.destroy();
    handle->current.destroy();
  };
};

int scheduler::getWakeTime(bool *flag) {
  uint64_t startTime = timerClock.now();
  int looptime = timerClock.getNext(startTime);
  while (looptime == 0) {
    auto clkWork = timerClock.get();
    auto *promise = getPromise<char>(clkWork);
    promise->setStatus(buffioRoutineStatus::executing);
    queue.push(clkWork);
    looptime = timerClock.getNext(startTime);
  };
  if (!queue.empty())
    return 0;
  if (queue.empty() && timerClock.empty()) {
    *flag = true;
    return 0;
  };
  if (timerClock.empty())
    return -1;
  return looptime;
};

int scheduler::yieldQueue(int chunk) {

  for (int i = chunk; 0 < i; i--) {
    if (queue.empty())
      return -1;

    auto handle = queue.get();
    auto promise = getPromise<char>(handle->current);
    if (promise->checkStatus() == buffioRoutineStatus::executing)
      handle->current.resume();

    switch (promise->checkStatus()) {
    case buffioRoutineStatus::executing:
      break;
    case buffioRoutineStatus::yield: {
      promise->setStatus(buffioRoutineStatus::executing);
    } break;
    case buffioRoutineStatus::waitingFd: {
      queue.erase();
    } break;
    case buffioRoutineStatus::paused: {
    } break;
    case buffioRoutineStatus::waiting: {
      queue.push(promise->getChild(), handle);
      queue.erase();
    } break;
    case buffioRoutineStatus::waitingTimer: {
      handle->current = nullptr;
      queue.pop();
    } break;
    case buffioRoutineStatus::pushTask: {
    } break;
    case buffioRoutineStatus::unhandledException:
      [[fallthrough]];

    case buffioRoutineStatus::error:
      [[fallthrough]];

    case buffioRoutineStatus::wakeParent: {
      if (handle->waiter != nullptr) {
        auto promiseP = getPromise<char>(handle->waiter->current);
        promiseP->setStatus(buffioRoutineStatus::executing);
        queue.push(handle->waiter);
        promise->setStatus(buffioRoutineStatus::paused);
        break;
      };
    }

      [[fallthrough]];
    case buffioRoutineStatus::zombie:
      [[fallthrough]];

    case buffioRoutineStatus::done: {
      handle->current.destroy();
      handle->current = nullptr;
      queue.pop();
      break;
    }
    };
    if (queue.empty())
      break;

    queue.mvNext();
  };
  return 0;
};

}; // namespace buffio
