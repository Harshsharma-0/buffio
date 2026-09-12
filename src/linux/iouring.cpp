#include "buffio/config.hpp"

#ifdef BUFFIO_BACKEND_IOURING

#include "buffio/core.hpp"
#include "buffio/fs.hpp"
#include "buffio/worker.hpp"
#include "buffio/ecode.hpp"


buffio::Worker::~Worker() { io_uring_queue_exit(&state.io.ring); };

int buffio::Worker::init(int numWorker) {
  return init(numWorker, (1U << BUFFIO_WORKER_QUEUE_ORDER));
};

int buffio::Worker::init(int numWorker, unsigned int queuesize) {

  /* round queue size to nearest power of 2, it queueSize is not pow2*/
  auto [size, order] = buffio::utility::get_pow2(queuesize);

  /* initlise the task_queue */
  if (init_task_queues(order) != 0)
    return B_EINITTSKQUE;

  if (init_poller(size) != 0)
    return B_EINITPOLLER;

  return 0;
};

int buffio::Worker::init_task_queues(unsigned int order) {
  return state.task_queue.init();
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

  if (unsubmitted <= 0)
    return 0;

  int cnt = io_uring_submit(ring);
  state.io.pending += cnt;

  return 0;
};

bool buffio::Worker::flush() {
  flush_io_requests(64);
  return true;
};

inline int handle_io_done(buffio::OpState &state, int32_t res) {

  buffio::AwaitableFileBase *base = (buffio::AwaitableFileBase *)state.data;

  switch (state.op_code) {
  case buffio::OpCode::Open: {
    int fd = static_cast<int>(res);
    if(res < 0)
       state.error = buffio::error_from_os(-res);

    state.fd = fd;
  } break;

  case buffio::OpCode::Read:
  case buffio::OpCode::Write:
  case buffio::OpCode::Readv:
  case buffio::OpCode::Writev:
   if (res < 0){
        state.error = buffio::error_from_os(-res);
        break;
   };
    state.op_done = static_cast<ssize_t>(res);
    *(base->state.poffset) += static_cast<uint64_t>(res);

  break;
  case buffio::OpCode::pRead:
  case buffio::OpCode::pWrite:
  case buffio::OpCode::pReadv:
  case buffio::OpCode::pWritev:
   if (res < 0){
        state.error = buffio::error_from_os(-res);
        break;
   };
    state.op_done = static_cast<ssize_t>(res);
  break;
  default:
    return 0;
  break;
  };

  return 1;
};

int buffio::Worker::flush_io_completed(unsigned int budget) {
  struct io_uring_cqe *cqe = nullptr;
  struct io_uring *ring = &state.io.ring;

  while (budget--) {

    if (io_uring_peek_cqe(ring, &cqe) < 0)
      break;
    if (cqe == NULL)
      break;

    buffio::OpState *obj = (buffio::OpState *)cqe->user_data;
    int32_t res = static_cast<int32_t>(cqe->res);
    assert(state.io.pending > 0);
    assert(obj);

    state.io.pending -= handle_io_done(*obj, res);

    io_uring_cqe_seen(&state.io.ring, cqe);
    state.task_queue.enqueue(obj->task);
  };

  return 0;
};

int buffio::Worker::wait_event() {

  struct io_uring *ring = &state.io.ring;
  unsigned int timeout =
      state.task_queue.empty() && state.io.pending != 0 ? 1 : 0;
  int count_done = io_uring_submit_and_wait(ring, timeout);

  if (count_done < 0 && errno != -EINTR) {
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
  abort_loop();
  return 0;
};

/* TODO: add abort loop on iouring */
void buffio::Worker::abort_loop(){};
/* TODO: add abort_io_completed on iouring */
void buffio::Worker::abort_io_completed(){};
/* TODO: add abort_io_requests on iouring */
void buffio::Worker::abort_io_requests(){};
/* TODO: add kill task on abort on iouring */
void buffio::Worker::kill_task_on_abort(){};
/* TODO: add abort_timers on iouring */
void buffio::Worker::abort_timers(){};

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
int buffio::Worker::init_worker_threads(int num) { return 0; };

/* for epoll backend only */
void buffio::Worker::wakeup_sleeping_workers() {};

#endif
