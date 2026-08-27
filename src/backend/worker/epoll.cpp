#include "buffio/worker.hpp"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <time.h>

static void buffioWorkerFunc(void *args);

struct WorkerParameters {
  int event_fd;
  std::atomic<int> *psleep_count;
  std::atomic<int> *pactive_count;
  std::atomic<buffio::LoopStatusCode> *pcontrol;
  std::latch *psync;
  WorkQueue *pwork_queue;
  WorkQueue *pcompletion_queue;
  SleepQueue *psleeping_queue;
  buffio::semaphore lock_self;
  buffio::thread thread;
};

static void buffioWorkerFunc(void *args) {

  WorkerParameters state = *(WorkerParameters *)args;
  buffio::semaphore *lock_self = &state.lock_self;

  state.pactive_count->fetch_add(1);
  state.psync->count_down();

  buffio::LoopStatusCode status_code =
      state.pcontrol->load(std::memory_order_acquire);

  while (status_code != buffio::LoopStatusCode::abort){

    std::optional<buffio::OpState *> op = state.pwork_queue->dequeue();

    if (!op) {
      state.psleep_count->fetch_add(1);
      state.psleeping_queue->enqueue(lock_self);
      lock_self->wait(); // wait for the semephore
      state.psleep_count->fetch_add(-1);
      status_code = state.pcontrol->load(std::memory_order_acquire);
      continue;
    };

    buffio::OpState *action = *op;
    action->action({nullptr, action->data});

    while (!state.pcompletion_queue->enqueue(action)){
      state.psleep_count->fetch_add(1);
      state.psleeping_queue->enqueue(lock_self);
      lock_self->wait(); // wait for the semephore
      state.psleep_count->fetch_add(-1);
    };

    /* insert code to signal back that work is done, or notify the main loop */
    status_code = state.pcontrol->load(std::memory_order_acquire);

    if (status_code == buffio::LoopStatusCode::inactive) {
      uint64_t evnt_op = (uint64_t)buffio::LoopStatusCode::event_wake;
      write(state.event_fd, (char *)&evnt_op, sizeof(uint64_t));
    }

  }; 

  state.pactive_count->fetch_add(-1);
  return;
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

int buffio::Worker::init(int numWorker) {
  return this->init(numWorker, (1U << BUFFIO_WORKER_QUEUE_ORDER));
};

int buffio::Worker::init_task_queues(unsigned int order) {
  /* initlising the sleeping Queue */
  if (!state.task_queue.init())
    return -1;
  if (!state.io.pending_queue.init())
    return -2;
  if (state.io.submit_queue.lfstart(order) != 0)
    return -3;
  if (state.io.completed.lfstart(order) != 0)
    return -4;
  if (state.event.sleeping_queue.lfstart(BUFFIO_SLEEP_QUEUE_ORDER) != 0)
    return -5;

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

  wparam = {
      state.event.event_fd,
      &state.sleep_count,
      &state.active_count,
      &state.control,
      &sync,
      &state.io.submit_queue,
      &state.io.completed,
      &state.event.sleeping_queue,
  };

  for (int i = 0; i < num; i++) {
    winfo[i] = wparam;

    if (winfo[i].lock_self.create(0) < 0)
      break;
    if (winfo[i].thread.run(buffioWorkerFunc, (void *)(winfo + i)) != 0) {
      winfo[i].lock_self.destroy();
      break;
    }
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

int buffio::Worker::init(int numWorker, unsigned int queueSize) {

  /* evaluating the maximum worker thread that can concurrently access the
   * queue*/
  auto [maxWorker, order] = buffio::utility::get_pow2(queueSize);

  /* checking it the maxWorker exceeds the maximun supported worker */
  maxWorker = maxWorker > BUFFIO_MAX_WORKER ? maxWorker : BUFFIO_MAX_WORKER;

  /* checking numWorker for negative value */
  numWorker = numWorker <= 0 ? 4 : numWorker;

  /* if numWorker exceed maxWorker set it to max worker */
  numWorker = numWorker > maxWorker ? maxWorker : numWorker;
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
  evnt.events = EPOLLIN | EPOLLET;
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
  
  //TODO notify tasks in the task queue
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
      flush_io_completed(64);
    };

    buffio::OpState *op = static_cast<buffio::OpState *>(evnt->data.ptr);

    /* perform socket read/write */
  };

  return 0;
};

void buffio::Worker::wakeup_sleeping_workers() {

  int sleeping = state.sleep_count.load(std::memory_order_acquire);
  size_t count = state.io.submit_queue.count();

  assert(sleeping >= 0);

  if (sleeping == 0)
    return;
  if (count == 0)
    return;

  size_t nwake =
      count >= (size_t)sleeping ? sleeping : ((size_t)sleeping - count);

  while (nwake--) {
    std::optional<buffio::semaphore *> sem =
        state.event.sleeping_queue.dequeue();
    if (!sem)
      break;
    (*sem)->post();
  };

  return;
};

int buffio::Worker::run_tasks(unsigned int budget) {
  while (budget--) {
    std::optional<buffio::CoroutineHandle> task = state.task_queue.dequeue();
    if (!task)
      break;
    task->resume();
  };
  return 0;
};

bool buffio::Worker::flush() {

  flush_io_requests(64);
  wakeup_sleeping_workers();
  flush_io_completed(64);
  flush_timers();
  return true;
};

int buffio::Worker::flush_io_completed(unsigned int budget) {

  while (budget--) {

    std::optional<buffio::OpState *> workd = state.io.completed.dequeue();
    if (!workd)
      break;

    if (!state.task_queue.enqueue((*workd)->task)) {
      return -1;
    }

    state.io.pending -= 1;
  };
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
      return 0;
    };
    pending += 1;
  };
  state.io.pending += pending;
  return 0;
};

bool buffio::Worker::push(buffio::OpState &vec) {
  state.io.pending_queue.enqueue(&vec);
  return true;
};

static void sleep_ms(unsigned long int ms){

  struct timespec ts;
  struct timespec rem;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;

  if(nanosleep(&ts,&rem) < 0){
    if(errno == EINTR)
       nanosleep(&rem,&ts);
  };

};

static void notify_abort_task(WorkQueue &queue,auto &task_queue){
  /* we will flush the completion queue first and notify tasks of abort */
  do{

   std::optional<buffio::OpState*> entry = queue.dequeue();
   if(!entry) break;

   (*entry)->op_done = 0; // present error abort;
   (*entry)->task.resume();                        

  }while(!queue.empty());

};

static void notify_workers_and_wait_abort(buffio::WorkerState &state){

  /* getting number of active worker doing works */
  int worker_active = state.active_count.load(std::memory_order_acquire);

  /* getting number of sleeping(inactive) workers doring works */
  int sleep_count = state.sleep_count.load(std::memory_order_acquire);
 
  /* waking up the sleeping workers so they can abort */
  while (sleep_count--) {
    std::optional<buffio::semaphore *> sem =
        state.event.sleeping_queue.dequeue();
    if (!sem) break;
    (*sem)->post(); // waking the workers asking for gracefull shutdown
  };

 unsigned int tries = 100;
 
 do{
   if(state.active_count.load(std::memory_order_acquire) == 0) break;
   sleep_ms(100);
 }while(tries--);

};

void buffio::Worker::abort_loop(){

  /* updating status to abort in contorl field of state */
  state.control.store(buffio::LoopStatusCode::abort,
      std::memory_order_release);

  notify_abort_task(state.io.completed,state.task_queue);
  notify_workers_and_wait_abort(state); 
  notify_abort_task(state.io.completed,state.task_queue);

};

void buffio::Worker::flush_timers() {

};
