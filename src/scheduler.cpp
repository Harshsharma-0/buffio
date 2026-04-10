#include "buffio/scheduler.hpp"
#include "buffio/enum.hpp"
#include "buffio/fiber.hpp"
#include "buffio/promise.hpp"
#include <atomic>
#include <unistd.h>

#define BUFFIO_FIBER_SETUP(AFTER_SETUP)                                        \
  buffio::fiber::poller = &this->poller;                                       \
  buffio::fiber::queue = &this->queue;                                         \
  buffio::fiber::timerClock = &this->timerClock;                               \
  buffio::fiber::requestBatch = &this->requestBatch;                           \
  buffio::fiber::threadRequestBatch = &this->threadRequestBatch;               \
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
  buffio::fiber::threadRequestBatch = nullptr;
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
  if ((error = buffio::MakeFd::eventFd(evFd, 0)) != 0)
    return error;
  if ((error = poller.pollMod(evFd.getFd(), &evFd, EPOLLIN | EPOLLET)) != 0) {
    evFd.release();
    return error;
  };
  workerlNum = workerNum;
  poller.mountFd(evFd.getFd());

  return 0;
}
int scheduler::run() {

  if (queue.empty())
    return (int)buffioErrorCode::unknown;

  struct epoll_event evnt[1024];

  bool exit = false;
  bool check = false;
  int timeout = 0;
  timerClock.pushExpired(queue);

  while (exit != true) {

    timeout = getWakeTime(&exit);
    int nfd = poller.poll(evnt, 1024, timeout);

    if (timeout < 0)
      buffio::fiber::loopWakedUp.compare_exchange_weak(
          check, true, std::memory_order_acq_rel);

    if (nfd < 0)
      break;
    if (nfd != 0)
      processEvents(evnt, nfd);

    dequeueThreadQueue(100);
    yieldQueue(100);

    if (!threadRequestBatch.empty())
      processThreadRequest();
    if (!requestBatch.empty())
      consumeBatch(100);
    if (!timerClock.empty())
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

    if (evnts[i].events & EPOLLIN)
      handle->takeEventReadAction();

    if (evnts[i].events & EPOLLOUT)
      handle->takeEventWriteAction();

    auto req = handle->getReserveHeader();
    if (req->isFresh) {
      requestBatch.push((buffioHeader *)req);
    }
  }
  return 0;
};

int scheduler::consumeBatch(int cycle) {

  int count = cycle < requestBatch.gcount() ? cycle : requestBatch.gcount();
  while (0 < count) {
    auto req = requestBatch.get();
    req->action(req);
    queue.push(req->entry);
    requestBatch.pop();
  };
  return 0;
};

void scheduler::cleanQueue() {
  while (!queue.empty()) {
    auto handle = queue.get();
    if (handle->waiter)
      handle->waiter->task.destroy(handle->waiter->task.storage);
    handle->task.destroy(handle->task.storage);
    queue.pop();
  };
};

#define _CHK(name) name.empty()

int scheduler::getWakeTime(bool *flag) {

  int looptime = timerClock.getNext();
  ssize_t nqueue =
      buffio::fiber::queuedCompleted.load(std::memory_order_acquire);
  bool n = true;

  if (_CHK(!queue) || _CHK(!requestBatch) || _CHK(!threadRequestBatch) ||
      nqueue > 0) {
    return 0;
  };

  ssize_t req = buffio::fiber::pendingReq.load(std::memory_order_acquire);
  if (req > 0) {
    if (looptime < 0)
      buffio::fiber::loopWakedUp.compare_exchange_weak(
          n, false, std::memory_order_acq_rel);

    return looptime;
  }

  *flag = (queue.empty() & timerClock.empty()) == true ? true : false;
  buffio::fiber::loopWakedUp.store(false, std::memory_order_release);

  return 0;
};
#undef _CHK

int scheduler::yieldQueue(int chunk) {

  int count = chunk < queue.gcount() ? chunk : queue.gcount();
  while (0 < count && !queue.empty()) {
    auto task = queue.get();
    task->task.run(task->task.storage);
  };
  return 0;
};

void scheduler::processThreadRequest() {

  size_t i = 0;

  while (!threadRequestBatch.empty()) {
    buffioHeader *header = threadRequestBatch.get();
    poller.push(header);
    threadRequestBatch.pop();
    i += 1;
  };

  buffio::fiber::pendingReq.fetch_add(i, std::memory_order_acq_rel);

  auto sleeping = buffio::fiber::sleepingThread.load(std::memory_order_acquire);
  ssize_t value = 0;
  size_t wakevalue = i;

  wakevalue = (i - sleeping) > 0 ? sleeping : i;

  for (ssize_t i = 0; i < wakevalue; i++) {
    poller.ping();
  }
};

void scheduler::dequeueThreadQueue(int nentry) {
  ssize_t nqueue =
      buffio::fiber::queuedCompleted.load(std::memory_order_acquire);
  ssize_t value = nqueue;
  if (value == 0)
    return;

  eventfd_t eval;
  eventfd_read(evFd.getFd(), &eval);

  if ((nentry - nqueue) < 0)
    value = nentry;

  for (ssize_t i = 0; i < value; i++) {
    auto header = poller.pop();
    queue.push(header->entry);
  };
  auto nvalue = buffio::fiber::queuedCompleted.fetch_sub(
      value, std::memory_order_acq_rel);

  return;
};
}; // namespace buffio
