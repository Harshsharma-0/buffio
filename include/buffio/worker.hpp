#ifndef BUFFIO_WORKER_HPP
#define BUFFIO_WORKER_HPP

#include "buffio/config.hpp"
#include "buffio/queue.hpp"
#include "buffio/lfqueue.hpp"
#include "buffio/thread.hpp"
#include "buffio/ecode.hpp"
#include "buffio/opcode.hpp"
#include <atomic>
#include <cstring>
#include <version>

#if defined(BUFFIO_BACKEND_EPOLL)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#elif defined(BUFFIO_BACKEND_IOURING)
#include<liburing.h>

#elif defined(BUFFIO_BACKEND_IOCP)
#include <synchapi.h>

#else 
  #error include/buffio/worker.hpp: macro error
#endif


using WorkQueue = buffio::lfQueue<buffio::OpState *>;

namespace buffio {

enum class LoopStatusCode : uint32_t {
  active = 0,
  abort = 1,
  inactive = 2,
  event_wake = 3,
  flush_submit = 4
};


struct WorkerArgs{
  buffio_fd sigfd;
  std::atomic<buffio::LoopStatusCode> *pcontrol;
  buffio::Latch *psync;
  WorkQueue *pwork_queue;
  WorkQueue *pcompletion_queue;
  WorkerSignal *pwork_lock;
  WorkerSignal *pcompletion_lock;  
};

struct WorkerThreadState {
  WorkerArgs args;
  buffio::thread thread;
};

struct EventState {
#if defined(BUFFIO_BACKEND_IOURING)

#elif defined(BUFFIO_BACKEND_EPOLL) || defined(BUFFIO_BACKEND_IOCP)
  buffio_fd evfd = BUFFIO_FD_INVALID;
  buffio_fd sigfd = BUFFIO_FD_INVALID; 

#else
#error Unsupported backend
#endif
};

struct IoState {

#if defined(BUFFIO_BACKEND_IOURING)

  struct io_uring ring;
  unsigned int ring_size = -1;
  size_t pending = 0;

#elif defined(BUFFIO_BACKEND_EPOLL) || defined(BUFFIO_BACKEND_IOCP)

  size_t pending = 0;
  WorkQueue completed;
  WorkQueue submit_queue;
  buffio::Queue<buffio::OpState *> pending_queue;

#else
#error Unsupported backend
#endif
};

struct WorkerState {
#if defined(BUFFIO_BACKEND_EPOLL) || defined(BUFFIO_BACKEND_IOCP)

  int worker_count = 0;
  uint32_t pending_commit = 0;
  std::atomic<LoopStatusCode> control = LoopStatusCode::active;

  WorkerSignal submit_lock;
  WorkerSignal completion_lock;

  WorkerThreadState *workers = nullptr;

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
  
  void abort_io_completed();
  void abort_io_requests();
  void kill_task_on_abort();
  void abort_timers();

  void flush_timers();
  bool should_exit();

  bool flush();

  static void WorkerThreadFunc(void *args);
  static void signalLoop(buffio_fd fd,LoopStatusCode code);
  WorkerState state;
};

using Instance = Worker;

}; // namespace buffio
#endif
