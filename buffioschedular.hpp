#ifndef BUFFIO_SCHEDULAR
#define BUFFIO_SCHEDULAR

/*
 * Error codes range reserved for buffioschedular
 *  [4000 - 5500]
 *  4000 <= errorcode <= 5500
 */

#if !defined(BUFFIO_IMPLEMENTATION)
#include "buffiopromsie.hpp"
#include "buffiosockbroker.hpp"
#endif

#include <cstring>
#include <exception>

class buffioschedular {

  class blockQueue {
  public:
    bool haveInstance = false;
    struct blockQueue *reserved;
    struct blockQueue *waiter;
    buffioroutine current;
    ~blockQueue() {
      if (haveInstance == true) {
        current.destroy();
      }
    }
  };

public:
  // constructor overload.
  buffioschedular()
      : queue(nullptr), head(0), tail(0), order(0), queueFilled(0) {};
  ~buffioschedular() { cleanUp(); };

  void cleanUp() {
    if (queue == nullptr)
      return;

    schedularMemory.release();
    delete[] queue;
    queue = nullptr;
  };
  // initliser of the schedular
  int init(buffioSockBrokerConfig _cfg, size_t execQueueOrder = 6) {
    if (execQueueOrder > 15)
      return -1;

    schedularMemory.init();
    try {
      queue = new blockQueue *[(1 << (execQueueOrder + 1))];
      order = (1 << (execQueueOrder + 1));
      ::memset(queue, '\0', (1 << order));

    } catch (std::exception &e) {
      return -1;
    };
    head = ~(0);
    tail = ~(0);
    queueFilled = 0;
    return 0;
  }

  int schedule(buffioroutine routine) {
    if (queue == nullptr)
      return -1;
    if (schedulePtr(routine) != nullptr)
      return 0;
    return -1;
  };

  void run() {
    if (queue == nullptr || queueFilled == 0)
      return;

    blockQueue *entry = nullptr;
    size_t tailLocal = 0;
    buffioroutine routine;
    buffiopromise *promise = nullptr;

    while (queueFilled > 10) {
      tailLocal = ++tail;
      entry = queue[tailLocal & (order - 1)];
      routine = entry->current;
      queue[tailLocal] = nullptr;
      promise = &routine.promise();

      if (promise->selfstatus.status == buffio_routine_status::executing)
        routine.resume();

      switch (promise->selfstatus.status) {
      case buffio_routine_status::executing:
        break;
      case buffio_routine_status::yield: {
        promise->setstatus(buffio_routine_status::executing);
        queue[((++head) & (order - 1))] = entry;
      } break;
      case buffio_routine_status::waiting_io: {
      } break;
      case buffio_routine_status::paused: {
      } break;
      case buffio_routine_status::waiting: {
        auto *task = schedulePtr(promise->handle);
        task->waiter = entry;
        queueFilled -= 1;
      } break;
      case buffio_routine_status::push_task: {
      } break;
      case buffio_routine_status::unhandled_exception:
      case buffio_routine_status::error:
      case buffio_routine_status::done:
        if (entry->waiter != nullptr) {
          auto &routinePromise = entry->waiter->current.promise();
          routinePromise.setstatus(buffio_routine_status::executing);
          routinePromise.childstatus = promise->selfstatus;
          schedulePtrInternal(entry->waiter);
        };
        entry->haveInstance = false;
        entry->current.destroy();
        schedularMemory.retMemory(entry);
        queueFilled -= 1;
        break;
      };
    }
  };

private:
  inline blockQueue *schedulePtr(buffioroutine routine) {
    if (queueFilled > order)
      return nullptr;

    auto *memory = schedularMemory.getMemory();
    if (memory == nullptr)
      return nullptr;

    memory->current = routine;
    memory->waiter = nullptr;
    queue[(++head & (order - 1))] = memory;
    queueFilled += 1;
    memory->haveInstance = true;

    return memory;
  };

  inline int schedulePtrInternal(blockQueue *ptrBlock) {
    if (queueFilled > order)
      return -1;

    queue[++head & (order - 1)] = ptrBlock;
    queueFilled += 1;
    return 0;
  };

  blockQueue **queue;
  blockQueue *pulledOut;
  size_t order;
  size_t head;
  size_t tail;
  size_t queueFilled; // -1 if empty, (N - 1) if there is N tasks
  buffioMemoryPool<blockQueue> schedularMemory;
  buffioSockBroker iopoller;
};
#endif
