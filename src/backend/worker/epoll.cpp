#include "buffio/worker.hpp"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

static void buffioWorkerFunc(void *args);

struct WorkerParameters {
  int event_fd;
  std::atomic<buffio::LoopStatusCode> *pcontrol;
  std::latch *psync;
  WorkQueue *pwork_queue;
  WorkQueue *pcompletion_queue;
  WorkerSignal *pwork_lock;
  WorkerSignal *psubmit_lock;
  buffio::thread thread;
};

static void buffioWorkerFunc(void *args) {
  auto &state = *static_cast<WorkerParameters *>(args);

  state.psync->count_down();

  for (;;) {

    state.pwork_lock->wait();

    auto status = state.pcontrol->load(std::memory_order_acquire);

    if (status == buffio::LoopStatusCode::abort)
      break;

    auto op = state.pwork_queue->dequeue();

    if (!op) {
      // Broken queue/signal invariant.
      continue;
    }

    auto *action = *op;
    action->action({nullptr, action->data});
    
    state.psubmit_lock->wait();
    state.pcompletion_queue->enqueue(action);

    status = state.pcontrol->load(std::memory_order_acquire);

    if (status == buffio::LoopStatusCode::inactive) {
      uint64_t event =
          static_cast<uint64_t>(buffio::LoopStatusCode::event_wake);

      ssize_t ret = write(state.event_fd, &event, sizeof(event));

      (void)ret;
    }
  }
  std::cout<<"exiting "<<std::endl;
};

buffio::Worker::~Worker() {

  if (state.event.epoll_fd >= 0)
    close(state.event.epoll_fd);
  if (state.event.event_fd >= 0)
    close(state.event.event_fd);
  if (!state.workers)
    return;

  delete[] static_cast<WorkerParameters *>(state.workers);
};

int buffio::Worker::init_task_queues(unsigned int order) {
  /* initlising the sleeping Queue */
  

  if (!state.task_queue.init())
    return -1;
  if (!state.io.pending_queue.init())
    return -2;
  if (state.io.submit_queue.lfstart(order) != 0)
    return -3;
  if (state.io.completed.lfstart(order) != 0) {
    return -4;
  }

  return 0;
};

int buffio::Worker::init_worker_threads(int num) {

  WorkerParameters *winfo = nullptr;
  WorkerParameters wparam = {};
  winfo = new (std::nothrow) struct WorkerParameters[num];

  if (winfo == nullptr) {
    return -1;
  };

  /* latch to ensure all resume execution only after all workers are initlised
   */
  std::latch sync{num};
  int nWorker = num;

  wparam = {state.event.event_fd,   &state.control,      &sync,
            &state.io.submit_queue, &state.io.completed, &state.submit_lock ,
            &state.completion_lock};
   
  for (int i = 0; i < num; i++) {
    winfo[i] = wparam;
    if (winfo[i].thread.run(buffioWorkerFunc, (void *)(winfo + i)) != 0)
      break;

    nWorker -= 1;
  };

  /* checking if createThread loop failed completely*/
  if (nWorker == num) {
    delete[] winfo;
    return -1;
  };
  /* auto caliberationg workerNum if the createThread loop failed partially */
  state.worker_count = num - nWorker;

  while (nWorker != 0) {
    /* decrements the sync counter to wait for worker to be executing */
    sync.count_down();
    nWorker -= 1;
  }

  /* checking and waiting for the worker to be ready for work */
  if (!sync.try_wait())
    sync.wait();
  state.workers = static_cast<void *>(winfo);

  return 0;
};

int buffio::Worker::init(int numWorker) {
  return this->init(numWorker, (1U << BUFFIO_WORKER_QUEUE_ORDER));
};

int buffio::Worker::init(int numWorker, unsigned int queueSize) {

  /* evaluating the maximum worker thread that can concurrently access the
   * queue*/
  auto [maxWorker, order] = buffio::utility::get_pow2(queueSize);

  /* checking it the maxWorker exceeds the maximun supported worker */
  maxWorker = maxWorker < BUFFIO_MAX_WORKER ? maxWorker : BUFFIO_MAX_WORKER;

  /* checking numWorker for negative value */
  numWorker = numWorker <= 0 ? 4 : numWorker;

  /* if numWorker exceed maxWorker set it to max worker */
  numWorker = numWorker > maxWorker ? maxWorker : numWorker;
  state.completion_lock.post(maxWorker);
  
  if (init_task_queues(order) != 0)
    return -1;
  if (init_poller(order) != 0)
    return -2;
  if (init_worker_threads(numWorker) != 0)
    return -3;

  return 0;
};

int buffio::Worker::init_poller(unsigned int order) {

  int epfd = epoll_create1(EPOLL_CLOEXEC);
  if (epfd < 0)
    return -1;

  int evntfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (evntfd < 0) {
    close(epfd);
    return -1;
  };

  struct epoll_event evnt;
  evnt.events = EPOLLIN;
  evnt.data.fd = evntfd;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, evntfd, &evnt) < 0) {
    close(epfd);
    close(evntfd);
    return -1;
  };

  state.event.epoll_fd = epfd;
  state.event.event_fd = evntfd;
  return 0;
};

int buffio::Worker::run() {

  while (!should_exit()) {
    flush_timers();
    wait_event();
    run_tasks(64);
    flush();
  };

   // TODO notify tasks in the task queue
    abort_loop();

  return 0;
};

bool buffio::Worker::should_exit() {
  return state.task_queue.empty() && state.io.pending == 0 ? true : false;
};

int buffio::Worker::wait_event() {

  int epoll_fd = state.event.epoll_fd;
  int event_fd = state.event.event_fd;
  constexpr int event_size = 1024;
  struct epoll_event events[1024];

  int timeout = state.task_queue.empty() && state.io.pending != 0 ? -1 : 0;
  if (timeout < 0)
    state.control.store(buffio::LoopStatusCode::inactive,
                        std::memory_order_release);
  int count = epoll_wait(epoll_fd, events, event_size, timeout);
  if (timeout < 0)
    state.control.store(buffio::LoopStatusCode::active,
                        std::memory_order_release);

  if (count < 0 && errno != EINTR)
    return -1;

  struct epoll_event *evnt = nullptr;
  for (int i = 0; i < count; i++) {
    evnt = (events + i);

    if (evnt->data.fd == event_fd) {
      uint64_t value;

      while (read(event_fd, &value, sizeof(value)) == sizeof(value)) {
        // Drain/coalesce notifications.
      }

      flush_io_completed(64);

      continue;
    }

    buffio::OpState *op = static_cast<buffio::OpState *>(evnt->data.ptr);

    /* perform socket read/write */
  };

  return 0;
};

void buffio::Worker::wakeup_sleeping_workers() { return; };

int buffio::Worker::run_tasks(unsigned int budget) {
  while (budget--) {
    std::optional<buffio::CoroutineHandle> task = state.task_queue.dequeue();
   
    if (!task){
      break;
    }
    
    task->resume();
  };
  return 0;
};

bool buffio::Worker::flush() {
  flush_io_requests(100);
  wakeup_sleeping_workers();
  flush_io_completed(100);
  flush_timers();
  return true;
};

int buffio::Worker::flush_io_completed(unsigned int budget) {

  unsigned int n = 0;
  while (budget--) {
  
    std::optional<buffio::OpState *> workd = state.io.completed.dequeue();
    if (!workd)
      break;
    
    //todo check dor abort and set op error to abort
    buffio::CoroutineHandle handle = (*workd)->task;
  
    if (!state.task_queue.enqueue(handle)) {
      return -1;
    };

    n += 1;
  };

  state.io.pending -= n;
  state.completion_lock.post(n);
  return 0;
};

int buffio::Worker::flush_io_requests(unsigned int budget) {

  int pending = 0;
  while (budget--) {

    std::optional<buffio::OpState *> workd = state.io.pending_queue.dequeue();
    if (!workd)
      break;

    if (!state.io.submit_queue.enqueue(*workd)) {
      state.io.pending_queue.enqueue(*workd);
      break;
    };
    pending += 1;
  };

  state.io.pending += pending;
  state.submit_lock.post(pending);

  return 0;
};

bool buffio::Worker::push(buffio::OpState &vec) {
  if(!state.io.pending_queue.enqueue(&vec)){
    return false;
  };
  return true;
};

static void sleep_ms(unsigned long int ms) {

  struct timespec ts;
  struct timespec rem;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;

  if (nanosleep(&ts, &rem) < 0) {
    if (errno == EINTR)
      nanosleep(&rem, &ts);
  };
};

void buffio::Worker::abort_loop() {

  /* updating status to abort in contorl field of state */
  state.control.store(buffio::LoopStatusCode::abort, std::memory_order_release);
  state.submit_lock.post(state.worker_count);

  flush_io_completed(-1);

  WorkerParameters *param = static_cast<WorkerParameters*>(state.workers);
  int workern = state.worker_count;
  
  for(int i = 0; i < workern;i++)
      param[i].thread.join();

  flush_io_completed(-1);
  flush_timers();
  run_tasks(-1); // runnning tasks to notify tasks of the error

};

void buffio::Worker::flush_timers() {

};
