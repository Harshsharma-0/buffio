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

using WorkQueue = buffio::lfQueue<buffio::OpState *>;

using SleepQueue =
    buffio::lfQueue<buffio::semaphore *, buffio::lfMemMode::stack,
                    BUFFIO_SLEEP_QUEUE_ORDER>;
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
  SleepQueue sleeping_queue;

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
  std::atomic<int> sleep_count = 0;
  std::atomic<int> active_count = 0;
  std::atomic<LoopStatusCode> control = LoopStatusCode::active;
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
