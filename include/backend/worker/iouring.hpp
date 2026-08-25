#ifndef BUFFIO_WORKER_IOURING_HPP
#define BUFFIO_WORKER_IOURING_HPP

#include "buffio/config.hpp"
#include "buffio/queue.hpp"
#include <liburing.h>
#include <linux/io_uring.h>
#include <atomic>

#ifndef BUFFIO_WORKER_IO_URING_ONLY
struct EventState{
 int epoll_fd = -1;
 buffio::Queue<buffio::OpState*> submit_queue;
};

#endif

struct IoState{
  struct io_uring ring;
  unsigned int ring_size = -1;
  size_t pending = 0;
};

struct WorkerState{
 #ifndef BUFFIO_IO_URING_ONLY
  EventState event;
 #endif
 IoState io;
 buffio::Queue<buffio::CoroutineHandle> task_queue;
};

namespace buffio{
class Worker{

  public:
    BUFFIO_CLASS_PROTECT(Worker);
    int init(int nworkers);
    int init(int nworkers,unsigned int queuesize);
    bool push(buffio::CoroutineHandle task) {
      return state.task_queue.enqueue(task);
    };
    bool push(buffio::OpState &vec);
    int run();
    ~Worker();
     Worker() = default;
  private:

  int flush_events();
  bool init_task_queue();
  bool init_event_queue();
  bool init_io_uring(int size);
  bool setup_events();
  bool should_exit();
  bool flush_io_uring();
  bool flush_epoll();
  void wait_events();
  void run_tasks(unsigned int budget);
  void process_completion(int cycle);
  void process_epoll_completion(){};

  WorkerState state;
};
};
#endif
