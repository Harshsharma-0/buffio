#ifndef BUFFIO_SCHEDULAR
#define BUFFIO_SCHEDULAR

/*
 * Error codes range reserved for buffioschedular
 *  [4000 - 5500]
 *  4000 <= errorcode <= 5500
 */

#include "buffiothread.hpp"
#if !defined(BUFFIO_IMPLEMENTATION)
#include "buffiopromsie.hpp"
#include "buffiosockbroker.hpp"
#endif

#include <cassert>
#include <exception>

class buffioScheduler {

  template <typename P> class blockQueue {
  public:
    struct blockQueue *reserved;
    struct blockQueue *waiter;
    buffioPromiseHandle<P> current;

    ~blockQueue() {
      if (current) {
        current.destroy();
      }
    }
  };

public:
  template <typename W> struct runRequest {
    buffioPromise<W> *task;
    size_t taskCount;
    bool async;
  };

  // constructor overload.
  buffioScheduler(buffioThreadView const &threadView) {
    threadPool = threadView;
  };
  ~buffioScheduler() = default;

  void cleanUp() {};
  int schedule() { return 0; };
  // dangerous temporary solution for check
  bool operator!() const { return true; }

  template <typename T>
  static int run(buffioScheduler::runRequest<T> &request,
                 buffioScheduler &instance) {
    if (!instance)
      return -1;

    assert(sizeof(buffioPromiseHandle<T>) == sizeof(void *));
    buffioMemoryPool<blockQueue<T>> schedulerMemory;

    if (request.async == true) {
      instance.threadPool()->run("buffio-schedular",
                                 buffioScheduler::__runInternal<T>, nullptr);
      return 0;
    };
    buffioScheduler::__runInternal<T>(nullptr);
    return 0;
  };

private:
  template <typename T> static int __runInternal(void *data) {
    buffioPromiseHandle<T> runQueue[1024]; // size = 2 ^ 10;

    return 0;
  };

  buffioThreadView threadPool; // storing as a refference
  buffioSockBroker iopoller;
};

/*
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
 */

#endif
