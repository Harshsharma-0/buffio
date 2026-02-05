#include "buffio/scheduler.hpp"

namespace buffio {
int scheduler::run() {
  if (queue.empty() && timerClock.empty())
    return -1;

  int error = 0;
  if ((error = headerPool.init()) != 0)
    return error;
  if ((error = poller.start(threadPool)) != 0)
    return error;
  if ((syncPipe = fdPool.get()) == nullptr)
    return -1;
  if ((error = buffioMakeFd::pipe(syncPipe)) != 0)
    return error;
  if ((error = poller.pollOp(syncPipe->getPipeRead(), syncPipe)) != 0) {
    syncPipe->release();
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
    if (nfd != 0)
      processEvents(evnt, nfd);

    yieldQueue(10); // yielding queue before batch consumption;
    consumeBatch(8);
    yieldQueue(10); // yielding queue after batch consumption;
    getWakeTime(&exit);
  };
  fdPool.release();
  cleanQueue();
  threadPool.free();
  return 0;
};

int scheduler::processEvents(struct epoll_event evnts[], int len) {
  for (int i = 0; i < len; i++) {
    auto handle = (buffioFd *)evnts[i].data.ptr;

    /*
     * below condition true if the fd is ready,
     * to read. if no request available the the
     * fd is just maked ready for read.
     */
    if (evnts[i].events & EPOLLIN) {
      auto req = handle->getReadReq();
      *handle | BUFFIO_READ_READY;
      if (req != nullptr) {
        if (evnts[i].events & EPOLLET) {
          requestBatch.push(req);
        } else {
          buffio::sockBroker::consumeEntry(req);
        };
      };
    }

    if (evnts[i].events & EPOLLOUT) {
      auto req = handle->getWriteReq();
      *handle | BUFFIO_WRITE_READY;

      if (req != nullptr) {
        if (evnts[i].events & EPOLLET) {
          requestBatch.push(req);
        } else {
          buffio::sockBroker::consumeEntry(req);
        };
      };
    };
    auto req = handle->getReserveHeader();
    if (req->opCode == buffioOpCode::none)
      continue;

    auto routine = buffio::sockBroker::handleAsync((buffioHeader *)req);
    if (handle)
      queue.push(routine);
  };
  // calls like accept and connect are handled here;
  return 0;
};

int scheduler::dispatchHandle(int errorCode, buffioFd *fd,
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
    fd->asyncReadDone(header);
    auto handle = header->onAsyncDone
                      .onAsyncWrite(errorCode, header->data.buffer,
                                    header->len.len, fd, header)
                      .get();
    queue.push(handle);
  } break;
  case buffioOpCode::asyncWrite: {
    fd->asyncWriteDone(header);
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
  if (queue.empty() && timerClock.empty() && fdPool.empty(1)) {
    *flag = true;
    return 0;
  };
  if (timerClock.empty())
    return -1;
  return looptime;
};

int scheduler::yieldQueue(int chunk) {
  if (queue.empty())
    return -1;

  for (int i = chunk; 0 < i; i--) {
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

int scheduler::push(buffioPromiseHandle handle) { return queue.push(handle); };

}; // namespace buffio
