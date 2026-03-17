#include "buffio/scheduler.hpp"
#include "buffio/enum.hpp"
#include "buffio/fiber.hpp"
#include "buffio/promise.hpp"
#include <atomic>
#include <iostream>
#include <unistd.h>

#define BUFFIO_FIBER_SETUP(AFTER_SETUP)                                        \
  buffio::fiber::poller = &this->poller;                                       \
  buffio::fiber::queue = &this->queue;                                         \
  buffio::fiber::timerClock = &this->timerClock;                               \
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
  buffio::fiber::requestBatch = nullptr;
};
void scheduler::clean(int tries, int timeout) {
  cleanQueue();
  shutWorker(workerlNum, tries, timeout);
  threadPool.free();
}
int scheduler::init(int workerNum, int queueOrder) {
  int error = 0;
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
    if (!requestBatch.empty())
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
      queue.push(header->routine);
      header->isFresh = false;
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
    if (req->isFresh){
         requestBatch.push((buffioHeader *)req);
    }

  }
  return 0;
};

int scheduler::consumeBatch(int cycle) {

  for (int i = 0; i < cycle; i++) {
    auto req = requestBatch.get();
    auto handle = req->action(req);
    queue.push(handle);
    requestBatch.pop();
    if (requestBatch.empty())
       return 0;

    std::cout<<requestBatch.gcount()<<std::endl;
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
    queue.pop();
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
   if (queue.empty())
      return 1;

  for (int i = chunk; 0 < i; i--) {
    auto handle = queue.get();
    auto promise = getPromise<char>(handle->current);
   
      if(promise->checkStatus() == buffioRoutineStatus::executing)
         handle->current.resume();
   

   auto status = promise->checkStatus(); 

    switch (status) {
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
    case buffioRoutineStatus::paused:{

    }break;
    case buffioRoutineStatus::clampThread: {
      auto header = promise->getAux<buffioHeader *>();
      poller.push(header);
      buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
      handle->current = nullptr;
      poller.ping();
      queue.pop();

    } break;
    case buffioRoutineStatus::waiting: {
      queue.push(promise->getChild(), handle);
      handle->current = nullptr;
      queue.pop();
    } break;
    case buffioRoutineStatus::waitingTimer: {
      handle->current = nullptr;
      promise->setStatus(buffioRoutineStatus::executing);
      queue.pop();
    } break;
    case buffioRoutineStatus::unhandledException:
      [[fallthrough]];

    case buffioRoutineStatus::error:
      [[fallthrough]];

    case buffioRoutineStatus::wakeParent: {
      if (handle->waiter != nullptr) {
        auto promiseP = getPromise<char>(handle->waiter->current);
        promiseP->setStatus(buffioRoutineStatus::executing);
        promise->setStatus(buffioRoutineStatus::paused);
        queue.push(handle->waiter);
        break;
      };
    };
    case buffioRoutineStatus::done: {
      handle->current.destroy();
      handle->current = nullptr;
      queue.pop();
      break;
    };
    case buffioRoutineStatus::backFromThread:{
      
     auto header = promise->getAux<buffioHeader *>();
     if(header->then != nullptr)
          queue.push(header->then);
          
      handle->current.destroy();
      handle->current = nullptr;
      queue.pop();
      delete header;    

      }break;
    };
    if (queue.empty())
      break;

    queue.mvNext();
  };
  return 0;
};

}; // namespace buffio
