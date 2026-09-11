#include "buffio/worker.hpp"
#include <iostream>

buffio::Worker::~Worker() {
 

 #if defined(BUFFIO_OS_LINUX)

  if (state.event.evfd >= 0)
    close(state.event.evfd);
  if (state.event.sigfd >= 0)
    close(state.event.sigfd);
  if (!state.workers)
    return;

 #elif defined(BUFFIO_OS_WINDOWS)
  if (state.event.evfd != BUFFIO_FD_INVALID)
    CloseHandle(state.event.evfd);

  if (!state.workers)
    return;

 #endif
  delete[] state.workers;
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

void buffio::Worker::abort_loop() {

  /* updating status to abort in contorl field of state */
  state.control.store(buffio::LoopStatusCode::abort, std::memory_order_release);
  state.submit_lock.post(state.worker_count);

  flush_io_completed(-1);

  WorkerThreadState *param = state.workers;
  int workern = state.worker_count;
  
  for(int i = 0; i < workern;i++)
      param[i].thread.join();

  flush_io_completed(-1);
  flush_timers();
  run_tasks(-1); // runnning tasks to notify tasks of the error

};

int buffio::Worker::init_worker_threads(int num) {

  WorkerThreadState *winfo = nullptr;
  WorkerArgs wparam = {};
  winfo = new (std::nothrow) struct WorkerThreadState[num];

  if (winfo == nullptr) {
    return -1;
  };

  /* latch to ensure all resume execution only after all workers are initlised
   */
  buffio::Latch sync{num};
  int nWorker = num;
  
  wparam.sigfd = state.event.sigfd;
  wparam.pcontrol = &state.control;
  wparam.psync = &sync;
  wparam.pwork_queue = &state.io.submit_queue;
  wparam.pwork_lock = &state.submit_lock;
  wparam.pcompletion_queue = &state.io.completed;
  wparam.pcompletion_lock = &state.completion_lock;


   
  for (int i = 0; i < num; i++) {
    winfo[i].args = wparam;
    if (winfo[i].thread.run(this->WorkerThreadFunc, (void *)(winfo + i)) != 0)
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
  sync.arrive_and_wait(nWorker);

  /* checking and waiting for the worker to be ready for work */
  state.workers = winfo;

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
  /* on windows initlize the IOCP*/
  if (init_poller(order) != 0)
    return -2;
  if (init_worker_threads(numWorker) != 0)
    return -3;

  return 0;
};

void buffio::Worker::flush_timers() {

};

void buffio::Worker::WorkerThreadFunc(void *args) {
  struct WorkerArgs state = *static_cast<struct WorkerArgs *>(args);

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
    
    state.pcompletion_lock->wait();
    state.pcompletion_queue->enqueue(action);

    status = state.pcontrol->load(std::memory_order_acquire);

    if (status == buffio::LoopStatusCode::inactive){
      Worker::signalLoop(state.sigfd,LoopStatusCode::event_wake); 
    }
    
  }
  std::cout<<"exiting "<<std::endl;
};
