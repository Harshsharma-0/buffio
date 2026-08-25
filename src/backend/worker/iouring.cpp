#include "buffio/config.hpp"
#include "buffio/core.hpp"
#include "buffio/defs.hpp"
#include "buffio/fs.hpp"
#include "buffio/queue.hpp"
#include "buffio/socket.hpp"
#include "buffio/worker.hpp"
#include <iostream>
#include <sys/epoll.h>

buffio::Worker::~Worker() {

  io_uring_queue_exit(&state.io.ring);

#ifndef BUFFIO_IO_URING_ONLY
  if (state.event.epoll_fd >= 0)
    close(state.event.epoll_fd);
#endif
};

int buffio::Worker::init(int numWorker) {
  return init(numWorker, (1U << BUFFIO_WORKER_QUEUE_ORDER));
};

int buffio::Worker::init(int numWorker, unsigned int queuesize) {

  /* round queue size to nearest power of 2, it queueSize is not pow2*/
  auto [size, order] = buffio::utility::get_pow2(queuesize);
  if (order == -1)
    return -1;

  /* initlise the task_queue */
  if (!init_task_queue())
    return -1;
#ifndef BUFFIO_IO_URING_ONLY

  if (!init_event_queue())
    return -1;

#endif

  if (!init_io_uring(size))
    return -1;

  return 0;
};

bool buffio::Worker::init_task_queue() {
  return state.task_queue.init(); 
};


bool buffio::Worker::init_event_queue() {

  if (!state.event.submit_queue.init())
    return false;
  
  /* create epoll fd */
  state.event.epoll_fd = epoll_create1(EPOLL_CLOEXEC);

  return state.event.epoll_fd >= 0;
};

bool buffio::Worker::init_io_uring(int size) {
  /* initlise io_uring */

  int error = io_uring_queue_init(size, &state.io.ring, 0);  

  if(error < 0){
#ifndef BUFFIO_IO_URING_ONLY
    close(state.event.epoll_fd);
    state.event.epoll_fd = -1;
#endif
    return false;
  };

  state.io.ring_size = size;

#ifndef BUFFIO_IO_URING_ONLY

  int epoll_fd = state.event.epoll_fd;
  int io_uring_fd = state.io.ring.ring_fd;

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = io_uring_fd;

  error = epoll_ctl(epoll_fd, EPOLL_CTL_ADD,io_uring_fd, &ev);
#endif


  return error == 0;
};

bool buffio::Worker::flush_io_uring(){
  struct io_uring *ring = &state.io.ring;


  unsigned int unsubmitted = io_uring_sq_ready(ring);

  if (unsubmitted > 0) {
    int cnt = io_uring_submit(ring);
    state.io.pending += cnt >= 0 ? cnt : 0;

  };

  return true;
};

bool buffio::Worker::flush_epoll(){

  ssize_t max_submit = static_cast<ssize_t>(state.io.ring_size);
  struct io_uring *ring = &state.io.ring;
  do {

    std::optional<buffio::OpState *> op = state.event.submit_queue.dequeue();

    /* when queue is empty */
    if (!op)
       break;

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

    /* better to check for null entry */
    if (sqe == NULL) {
       flush_io_uring();
      sqe = io_uring_get_sqe(ring);
    };

    (*op)->action({static_cast<void *>(sqe), static_cast<void *>((*op)->data)});

    // check if the io_uring at max capacity and submit
    if (io_uring_sq_space_left(ring) == 0)
       flush_io_uring();


    max_submit -= 1;

  } while (max_submit > 0);


  flush_io_uring();


  return true;
};

int buffio::Worker::flush_events() {
  flush_io_uring();

#ifndef BUFFIO_IO_URING_ONLY
  flush_epoll();
#endif
  return 0;
};

void buffio::Worker::process_completion(int cycle) {
  struct io_uring_cqe *cqe = nullptr;

  do {
    if (io_uring_peek_cqe(&state.io.ring, &cqe) < 0)
      break;
    buffio::OpState *obj = (buffio::OpState *)cqe->user_data;
    int32_t res = static_cast<int32_t>(cqe->res);

    assert(state.io.pending > 0);
    assert(obj);

    obj->op_done = res;
    state.io.pending -= 1;

    io_uring_cqe_seen(&state.io.ring, cqe);
    state.task_queue.enqueue(obj->task);

    cycle -= 1;
  } while (cycle > 0);
};

void buffio::Worker::wait_events(){

#ifndef BUFFIO_IO_URING_ONLY

    int timeout = state.task_queue.empty() ? -1 : 0;
    int epoll_fd = state.event.epoll_fd;
    constexpr int event_max = 1024;
    struct epoll_event event[event_max];

    int count_done = epoll_wait(epoll_fd, event, event_max, timeout);
    if (count_done < 0 && errno != EINTR) {
      std::cout << "[epoll error] " << strerror(errno) << std::endl;
      return;
    }
    for (int idx = 0; idx < count_done; idx++) {

      struct epoll_event *evnt = (event + idx);
      if (evnt->data.fd == state.io.ring.ring_fd) {
         process_completion(count_done);
         continue;
      };

      /*
         handle socket read write
      */
    };

#else
    int timeout = state.task_queue.empty() ? 1 : 0;
    int count_done = io_uring_submit_and_wait(ring, timeout);
    if (count_done < 0 && errno != -EINTR) {
      std::cout << "[io uring error] " << strerror(-errno) << std::endl;
      return ;
    };

    process_completion(count_done);

#endif

};

void buffio::Worker::run_tasks(unsigned int budget){

 std::optional<buffio::CoroutineHandle> task;

 while(budget--){
      task = state.task_queue.dequeue();
      if (!task) {
        break;
      }

      task->resume();    
 };

};
int buffio::Worker::run() {

  assert(!state.task_queue.empty());
  int task_exec_cycle = 64;
  
  while(!should_exit()){
    wait_events();
    run_tasks(task_exec_cycle);
    flush_events();
  };

  return 0;
};

bool buffio::Worker::should_exit(){
return state.task_queue.empty() && (state.io.pending == 0)
             ? true : false;
};

bool buffio::Worker::push(buffio::OpState &vec) {

#ifndef BUFFIO_IO_URING_ONLY
  if (!state.event.submit_queue.enqueue(&vec))
    return false;

  if (state.event.submit_queue.count() > 10)
       flush_events();

#else

  struct io_uring *ring = &state.io.ring;
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

  /* check for full queue */
  if (sqe == NULL) {
    flush_events();
    sqe = io_uring_get_sqe(ring);
  };

  vec.action({static_cast<void *>(sqe), static_cast<void *>(vec.data)});

#endif

  return true;
};
