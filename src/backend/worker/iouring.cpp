#include "buffio/config.hpp"
#include "buffio/core.hpp"
#include "buffio/fs.hpp"
#include "buffio/worker.hpp"
#include <iostream>

buffio::Worker::~Worker() { io_uring_queue_exit(&state.io.ring); };

int buffio::Worker::init(int numWorker) {
  return init(numWorker, (1U << BUFFIO_WORKER_QUEUE_ORDER));
};

int buffio::Worker::init(int numWorker, unsigned int queuesize) {

  /* round queue size to nearest power of 2, it queueSize is not pow2*/
  auto [size, order] = buffio::utility::get_pow2(queuesize);
  if (order == -1)
    return -1;

  /* initlise the task_queue */
  if (init_task_queues(order) != 0)
    return -1;

  if (init_poller(size) != 0)
    return -1;

  return 0;
};

int buffio::Worker::init_task_queues(unsigned int order) {
  return !state.task_queue.init() ? -1 : 0;
};

int buffio::Worker::init_poller(unsigned int size) {
  /* initlise io_uring */

  int error = io_uring_queue_init(size, &state.io.ring, 0);

  if (error < 0) {
    return -1;
  };

  state.io.ring_size = size;
  return 0;
};

int buffio::Worker::flush_io_requests(unsigned int budget) {
  struct io_uring *ring = &state.io.ring;

  unsigned int unsubmitted = io_uring_sq_ready(ring);

  if (unsubmitted <= 0) return 0;

   int cnt = io_uring_submit(ring);
   state.io.pending += cnt;

  return 0;
};

bool buffio::Worker::flush() {
  flush_io_requests(64);
  return true;
};

int buffio::Worker::flush_io_completed(unsigned int budget) {
  struct io_uring_cqe *cqe = nullptr;
  struct io_uring *ring = &state.io.ring;

  while(budget--) {
    
    if(io_uring_peek_cqe(ring,&cqe) < 0) break;
    if(cqe == NULL) break;
    buffio::OpState *obj = (buffio::OpState *)cqe->user_data;
    int32_t res = static_cast<int32_t>(cqe->res);

    assert(state.io.pending > 0);
    assert(obj);

    obj->op_done = res;
    state.io.pending -= 1;

    io_uring_cqe_seen(&state.io.ring, cqe);
    state.task_queue.enqueue(obj->task);

  };

  return 0;
};

int buffio::Worker::wait_event() {

  struct io_uring *ring = &state.io.ring;
  unsigned int timeout = state.task_queue.empty() && state.io.pending != 0 ? 1 : 0;
  int count_done = io_uring_submit_and_wait(ring, timeout);
  
  if (count_done < 0 && errno != -EINTR) {
    std::cout << "[io uring error] " << strerror(-errno) << std::endl;
    return -1;
  };

  flush_io_completed(64);
  return 0;
};

int buffio::Worker::run_tasks(unsigned int budget) {

  std::optional<buffio::CoroutineHandle> task;

  while (budget--) {
    task = state.task_queue.dequeue();
    if (!task) {
      break;
    }
    task->resume();
  };
  return 0;
};

int buffio::Worker::run() {

  assert(!state.task_queue.empty());
  int task_exec_cycle = 64;

  while (!should_exit()) {
    wait_event();
    run_tasks(task_exec_cycle);
    flush_io_requests(64);
  };

  return 0;
};

bool buffio::Worker::should_exit() {
  return state.task_queue.empty() && (state.io.pending == 0) ? true : false;
};

bool buffio::Worker::push(buffio::OpState &vec) {

  struct io_uring *ring = &state.io.ring;
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

  /* check for full queue */
  if (sqe == NULL) {
    flush();
    sqe = io_uring_get_sqe(ring);
  };
  
  vec.action({static_cast<void *>(sqe), static_cast<void *>(vec.data)});
  return true;
};


/* for epoll backend only */
int buffio::Worker::init_worker_threads(int num){ return 0;};

/* for epoll backend only */
void buffio::Worker::wakeup_sleeping_workers(){};

