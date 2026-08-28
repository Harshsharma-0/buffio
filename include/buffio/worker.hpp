#ifndef BUFFIO_WORKER_HPP
#define BUFFIO_WORKER_HPP

#include "buffio/config.hpp"
#include "buffio/queue.hpp"
#include <atomic>
#include <cstring>
#include <latch>

#if defined(BUFFIO_BACKEND_EPOLL)
#include "buffio/lfqueue.hpp"
#include "buffio/queue.hpp"
#include "buffio/thread.hpp"
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <iostream>
using WorkQueue = buffio::lfQueue<buffio::OpState *>;

class WorkerSignal{
  private:
  std::atomic<uint32_t> work_count = 0;
  std::atomic<uint32_t> sleeping_count = 0;
  
  static inline int futex_wait(std::atomic<uint32_t>* addr, uint32_t expected){
   return syscall(SYS_futex,(uint32_t*)addr,FUTEX_WAIT_PRIVATE,expected,NULL,NULL,0);
  };
  static inline int futex_wake(std::atomic<uint32_t>* addr, uint32_t num_threads){
    return syscall(SYS_futex,(uint32_t*)addr,FUTEX_WAKE_PRIVATE,num_threads);
  };
  public:

  void post(uint32_t n){
    work_count.fetch_add(n,std::memory_order_release);
    uint32_t inactive = sleeping_count.load(std::memory_order_acquire);
    int minWake = inactive > n ? n : inactive;
    WorkerSignal::futex_wake(&work_count,(uint32_t)minWake);
  };

  void wait(){

    for(;;){
     uint32_t w_cnt = work_count.load(std::memory_order_acquire);

     /* try acquiring a slot in the work */
     while(w_cnt > 0){
       if(work_count.compare_exchange_weak(
                 w_cnt,w_cnt - 1,
                 std::memory_order_acquire,
                 std::memory_order_acquire)){
         return;
       }
     };
     
     /* announce that we are going to be sleeping */
     sleeping_count.fetch_add(1,std::memory_order_relaxed);
     
     // checking for work once more
     w_cnt = work_count.load(std::memory_order_acquire);

     
     if(w_cnt != 0){
         sleeping_count.fetch_sub(1,std::memory_order_relaxed);
         continue;
     };
        
       WorkerSignal::futex_wait(&work_count,0);
        /* code to wait */
       sleeping_count.fetch_sub(1,std::memory_order_relaxed);
    };
  };
};

#endif

#if defined(BUFFIO_BACKEND_IOURING)
 #include <liburing.h>
#endif

namespace buffio {

enum class LoopStatusCode : uint32_t {
  active = 0,
  abort = 1,
  inactive = 2,
  event_wake = 3,
  flush_submit = 4
};


struct EventState {
#if defined(BUFFIO_BACKEND_IOURING)

#elif defined(BUFFIO_BACKEND_EPOLL)

  int epoll_fd = -1;
  int event_fd = -1;

#elif defined(BUFFIO_BACKEND_IOCP)

#else
#error Unsupported backend
#endif
};

struct IoState {

#if defined(BUFFIO_BACKEND_IOURING)

  struct io_uring ring;
  unsigned int ring_size = -1;
  size_t pending = 0;

#elif defined(BUFFIO_BACKEND_EPOLL)

  size_t pending = 0;
  WorkQueue completed;
  WorkQueue submit_queue;
  buffio::Queue<buffio::OpState *> pending_queue;

#elif defined(BUFFIO_BACKEND_IOCP)

#else
#error Unsupported backend
#endif
};

struct WorkerState {

#if defined(BUFFIO_BACKEND_IOURING)

#elif defined(BUFFIO_BACKEND_EPOLL)

  int worker_count = 0;
  uint32_t pending_commit = 0;
  std::atomic<LoopStatusCode> control = LoopStatusCode::active;
  WorkerSignal locked;
  void *workers = nullptr;

#elif defined(BUFFIO_BACKEND_IOCP)

#else
#error Unsupported backend
#endif

  EventState event;
  IoState io;
  buffio::Queue<buffio::CoroutineHandle> task_queue;
};

class Worker {

public:
  BUFFIO_CLASS_PROTECT(Worker);
  int init(int nworkers);
  int init(int nworkers, unsigned int queuesize);
  bool push(buffio::CoroutineHandle task) {
    return state.task_queue.enqueue(task);
  };
  bool push(buffio::OpState &vec);
  int run();
  ~Worker();
  Worker() = default;

private:

 

  int init_poller(unsigned int order);
  int init_task_queues(unsigned int order);
  int flush_io_completed(unsigned int budget);
  int flush_io_requests(unsigned int budget);
  int run_tasks(unsigned int budget);

  /* for epoll backend only */
  int init_worker_threads(int num);
  /* for epoll backend only */
  void wakeup_sleeping_workers();
  /* for epoll backend only */
  void abort_loop();
  int wait_event();

  void flush_timers();
  bool should_exit();

  bool flush();

  WorkerState state;
};

using Instance = Worker;

}; // namespace buffio
#endif
