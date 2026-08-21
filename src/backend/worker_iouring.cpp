#include "buffio/file.hpp"
#include "buffio/queue.hpp"
#include "buffio/worker.hpp"
#include <sys/epoll.h>
#include <iostream>

buffio::Worker::Worker() {

 state_.ring_size = 0;
 state_.fd = -1;

};
buffio::Worker::~Worker() {

 io_uring_queue_exit(&state_.ring);
 if(state_.fd >= 0) 
   close(state_.fd);

};

int buffio::Worker::init(int numWorker) {
  return this->init(numWorker, (1U << BUFFIO_WORKER_QUEUE_ORDER));
};

int buffio::Worker::init(int numWorker, unsigned int queueSize) {
  
  /* round queue size to nearest power of 2, it queueSize is not pow2*/
  auto[size,order] = buffio::utility::get_pow2(queueSize);
  if (order == -1)
    return -1;
 
  /* initlise the task_queue */
  if(!task_queue.init()) return -1;

  /* create epoll fd */
  if((state_.fd = epoll_create1(EPOLL_CLOEXEC)) < 0) return -1;

  /* initlise io_uring */
  if (io_uring_queue_init(size, &state_.ring, 0) != 0){ 
    close(state_.fd);
    return -1; 
  };
   
   /* store the ring size for future use */
   state_.ring_size = size;
   return 0;
};

int buffio::Worker::flush() {

  struct io_uring *ring = &state_.ring;
  ssize_t max_submit = static_cast<ssize_t>(state_.ring_size);

  /* check the ring and submit the remaining entry 
   * before making new entry
   */

  unsigned int unsubmitted = io_uring_sq_ready(ring);

  if(unsubmitted > 0)
     io_uring_submit(ring);
     
  do{

    /* op type = buffio::op_vec , and ecode type = int */
    auto op = state_.submit_queue.dequeue();
   
    /* when queue is empty */
    if(!op) break;
  
   
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    
    /* better to check for null entry */
    if(sqe == NULL){
      state_.submit_queue.enqueue(*op);
      return 1; /* op failed at a point */
    }

    /* continue consuming the queue */
    buffio::dispatch_op(*op,static_cast<void *>(ring));

    
   // check if the io_uring at max capacity and submit
   if (io_uring_sq_space_left(ring) == 0)
         unsigned stotal = io_uring_submit(ring); // notify kenal of events

    max_submit -= 1;
  }while(max_submit > 0);


  /* better to check to un-reported event to the kernel */
  unsubmitted = io_uring_sq_ready(ring);
  if(unsubmitted > 0)
     io_uring_submit(ring);

  return 0;
};


int buffio::Worker::run(){

 std::optional<buffio::vTask> task;
 bool exit = false;
 do{
   task = task_queue.dequeue();

   if(!task) break;

   task->resume();
 }while(!exit);

 return 0;
};

bool buffio::Worker::push(buffio::op_vec vec) {
  
 if (state_.submit_queue.enqueue(vec))
     return true;

 int max_tries = 100;

 do{

    this->flush(); // flush the queue
    
    // try submitting once more time
    if (state_.submit_queue.enqueue(vec)) return true;
    
    // count downt he tries
    max_tries -= 1;

  }while(0 < max_tries);

  return false;
};
