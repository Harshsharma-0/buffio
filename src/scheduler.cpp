#include "buffio/scheduler.hpp"
#include "buffio/enum.hpp"
#include <iostream>
#include <unistd.h>

#define BUFFIO_FIBER_SETUP(AFTER_SETUP)                                        \
  buffio::fiber::poller = &this->poller;                                       \
  buffio::fiber::queue = &this->queue;                                         \
  buffio::fiber::timerClock = &this->timerClock;                               \
  buffio::fiber::headerPool = &this->headerPool;                               \
  buffio::fiber::requestBatch = &this->requestBatch;                           \
  AFTER_SETUP

namespace buffio {

scheduler::scheduler() : workerlNum(0) { BUFFIO_FIBER_SETUP() };

scheduler::scheduler(int workerNum, int queueOrder) : workerlNum(0) {
  BUFFIO_FIBER_SETUP(
      if ((workerlNum = init(workerNum, queueOrder)) < 0) return);
  workerlNum = workerNum;
};
scheduler::~scheduler() {
  buffio::fiber::poller = nullptr;
  buffio::fiber::queue = nullptr;
  buffio::fiber::timerClock = nullptr;
  buffio::fiber::headerPool = nullptr;
  buffio::fiber::requestBatch = nullptr;
};
void scheduler::clean(int tries, int timeout) {
  cleanQueue();
  shutWorker(workerlNum, tries, timeout);
  threadPool.free();
}
int scheduler::init(int workerNum, int queueOrder) {
  int error = 0;
  if ((error = headerPool.init()) != 0)
    return error;
  if ((error = poller.start(threadPool, workerNum, queueOrder)) != 0)
    return error;
  if ((error = buffio::MakeFd::pipe(syncPipe)) != 0)
    return error;
  if ((error = poller.pollMod(syncPipe.getPipeRead(), NULL, EPOLLIN)) != 0) {
    syncPipe.release();
    return error;
  };
  poller.mountWakeFd(syncPipe.getPipeWrite());
  return 0;
}
int scheduler::run() {

  if (queue.empty())
    return (int)buffioErrorCode::unknown;

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
    timerClock.pushExpired(queue);
    if (exit == true)
      break;
    int nfd = poller.poll(evnt, 1024, timeout);

    if (nfd < 0)
      break;
    if (nfd != 0) {
      processEvents(evnt, nfd);
    }
    if (yieldQueue(10) < 0)
      break;
    consumeBatch(8);
    if (yieldQueue(10) < 0)
      break;
    timerClock.pushExpired(queue);
  };
  return 0;
};

void scheduler::shutWorker(int workerNum, int tries, long wait) {

  size_t workerCount =
      buffio::fiber::workerCount.load(std::memory_order_acquire);

  buffio::fiber::abort.store(-10, std::memory_order_release);

  for (size_t j = 0; j < workerCount; j++)
    poller.ping();

  struct timespec ts;
  ts.tv_sec = wait / 1000;
  ts.tv_nsec = (wait % 1000) * 100000L;

  ::nanosleep(&ts, &ts);
  workerCount = buffio::fiber::workerCount.load(std::memory_order_acquire);

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
    if (handle == nullptr) {
      char s[2];
      read(syncPipe.getPipeRead(), s, 1);
      auto header = poller.pop();
      dispatchHandle(header->opError, header->fd, header);
      continue;
    };
    if (evnts[i].events & EPOLLIN) {
      auto req = handle->getPendingRead();

      if (req != nullptr) {
        buffio::fiber::pendingReq.fetch_add(-1, std::memory_order_acq_rel);
        handle->popPendingRead();
        requestBatch.push(req);
      } else {
        *handle | BUFFIO_READ_READY;
      }
    }

    if (evnts[i].events & EPOLLOUT) {
      auto req = handle->getPendingWrite();
      if (req != nullptr) {
        buffio::fiber::pendingReq.fetch_add(-1, std::memory_order_acq_rel);

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
      buffio::fiber::pendingReq.fetch_add(-1, std::memory_order_acq_rel);
      break;
    case buffioOpCode::wakeOnReadReady:
      if (handle->isBitSet(BUFFIO_READ_READY))
        queue.push(req->routine);
    case buffioOpCode::wakeOnWriteReady:
      if (handle->isBitSet(BUFFIO_WRITE_READY))
        queue.push(req->routine);
      break;
    };
    if (req->rwtype == buffioReadWriteType::async) {
      auto routine = buffio::sockBroker::handleAsync((buffioHeader *)req);
      if (routine) {
        buffio::fiber::pendingReq.fetch_add(-1, std::memory_order_acq_rel);
        queue.push(routine);
      }
    }
  };
  return 0;
};

int scheduler::dispatchHandle(int errorCode, buffio::Fd *fd,
                              buffioHeader *header) {
  switch (header->opCode) {
  case buffioOpCode::read:
    [[fallthrough]];
  case buffioOpCode::readFile:
    [[fallthrough]];
  case buffioOpCode::write:
    [[fallthrough]];
  case buffioOpCode::writeFile:
    queue.push(header->routine);
    break;
  case buffioOpCode::asyncReadFile:
  case buffioOpCode::asyncRead: {
    auto handle = header->onAsyncDone
                      .onAsyncRead(errorCode, header->data.buffer,
                                    header->len.len, fd)
                      .get();
    headerPool.push(header);
    queue.push(handle);
  } break;

  case buffioOpCode::asyncWriteFile:
  case buffioOpCode::asyncWrite: {
    auto handle = header->onAsyncDone
                      .onAsyncWrite(errorCode, header->data.buffer,
                                   header->len.len, fd)
                      .get();
    headerPool.push(header);
    queue.push(handle);
  } break;
  case buffioOpCode::clampThread:
    headerPool.push(header);
    return 0;
    break;
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
    if (req->rwtype == buffioReadWriteType::async) {

      auto routine = buffio::sockBroker::handleAsync(req);
      if (routine)
        queue.push(routine);

      requestBatch.pop();
    } else {

      int error = buffio::sockBroker::consumeEntry(req);
      dispatchHandle(error, req->fd, req);
      requestBatch.pop();
      if (requestBatch.empty())
        break;
      requestBatch.mvNext();
      return 0;
    };
  }
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

  int looptime = timerClock.getNext();
  if (!queue.empty() || !requestBatch.empty()) {
    return 0;
  };
  ssize_t req = buffio::fiber::pendingReq.load(std::memory_order_acquire);
  if (req > 0) {
    return looptime;
  }

  if (queue.empty() && timerClock.empty()) {
    *flag = true;
    return 0;
  };
  return looptime;
};

int scheduler::yieldQueue(int chunk) {
  for (int i = chunk; 0 < i; i--) {
    if (queue.empty())
      return 1;

    auto handle = queue.get();
    auto promise = getPromise<char>(handle->current);
    if (promise->checkStatus() != buffioRoutineStatus::executing) {
      std::cout << "[invalid entry found]" << std::endl;
      return -1;
    };
    handle->current.resume();

    switch (promise->checkStatus()) {
    case buffioRoutineStatus::executing:
      break;
    case buffioRoutineStatus::yield: {
    } break;
    case buffioRoutineStatus::waitingFd: {
      promise->setStatus(buffioRoutineStatus::executing);
      handle->current = nullptr;
      queue.pop();
    } break;
    case buffioRoutineStatus::waitingFile: {
      promise->setStatus(buffioRoutineStatus::executing);
      handle->current = nullptr;
      queue.pop();
      poller.ping();
      break;
    };
    case buffioRoutineStatus::clampThread:{
      queue.pop();
      auto header = promise->getAux<buffioHeader*>(); 
      poller.push(header);
      buffio::fiber::pendingReq.fetch_add(1,std::memory_order_acq_rel);
      handle->current = nullptr;
      poller.ping();
    }break;
    case buffioRoutineStatus::paused: {
    } break;
    case buffioRoutineStatus::waiting: {
      queue.push(promise->getChild(), handle);
      handle->current = nullptr;
      queue.pop();
    } break;
    case buffioRoutineStatus::waitingTimer: {
      handle->current = nullptr;
      promise->setStatus(buffioRoutineStatus::executing);
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
    };
    };
    if (queue.empty())
      break;

    queue.mvNext();
  };
  return 0;
};

}; // namespace buffio
