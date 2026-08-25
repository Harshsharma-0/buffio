#ifndef BUFFIO_WORKER_DEFAULT
#define BUFFIO_WORKER_DEFAULT

#include "buffio/lfqueue.hpp"
#include "buffio/queue.hpp"
#include "buffio/thread.hpp"
#include "buffio/core.hpp"

#include <cstring>
#include <atomic>
#include <latch>


using WorkQueue= 
     buffio::lfQueue<buffio::OpState *>;

using SleepQueue = buffio::lfQueue<std::variant<buffio::semaphore*,int>,
                        buffio::lfMemMode::stack,BUFFIO_SLEEP_QUEUE_ORDER>;

struct IoState{
    size_t pending = 0;
    WorkQueue completed;
    WorkQueue submit_queue;
    buffio::Queue<buffio::OpState *> ready_queue;
};

struct EventState{
    int epoll_fd = -1;
    int event_fd = -1;
    SleepQueue sleeping_queue;
};

struct WorkerState {

    int worker_count = 0;
    std::atomic<int> sleep_count = 0;
    std::atomic<int> active_count = 0;
    std::atomic<int> control = 0; 
    void* workers = nullptr;
    buffio::Queue<buffio::CoroutineHandle> task_queue;

    IoState io;
    EventState event;
   
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

  bool should_exit();
  int wait_event();
  int run_tasks(unsigned int budget);
  int flush_io_requests();
  int flush_io_comleted();
  int init_task_queues(unsigned int order);
  int init_worker_threads(int num);
  int init_epoll_event();

  bool flush(); 
  WorkerState state;
};
};

#endif
