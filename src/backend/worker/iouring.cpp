#include "buffio/core.hpp"
#include "buffio/defs.hpp"
#include "buffio/file.hpp"
#include "buffio/socket.hpp"
#include "buffio/queue.hpp"
#include "buffio/worker.hpp"
#include "buffio/file.hpp"
#include <sys/epoll.h>
#include <iostream>

buffio::Worker::Worker() {

 state_.ring_size = 0;
 state_.fd = -1;
 state_.io_count = 0;
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
  if(!state_.submit_queue.init()) return -1;

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

  if(unsubmitted > 0){
     int cnt = io_uring_submit(ring);
     state_.io_count += cnt >= 0 ? cnt : state_.io_count;
    };

  do{

    /* op type = std::optional<buffio::op_vec> */
    std::optional<buffio::op_vec> op = state_.submit_queue.dequeue();

    /* when queue is empty */
    if(!op){ 
      break;
    }
  
   
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    
    /* better to check for null entry */
    if(sqe == NULL){
      state_.submit_queue.enqueue(*op);
      return 1; /* op failed at a point */
    }

    /* continue consuming the queue */
    buffio::dispatch_op(*op,static_cast<void *>(sqe));

   // check if the io_uring at max capacity and submit
   if (io_uring_sq_space_left(ring) == 0){
     int cnt = io_uring_submit(ring);
     state_.io_count += cnt >= 0 ? cnt : state_.io_count;
    }

    max_submit -= 1;
  }while(max_submit > 0);


  /* better to check to un-reported event to the kernel */
  unsubmitted = io_uring_sq_ready(ring);

  if(unsubmitted > 0){
     int cnt = io_uring_submit(ring);
     state_.io_count += cnt >= 0 ? cnt : state_.io_count;
   }
  
  return 0;
};


void buffio::Worker::consume_done(int cycle){
  struct io_uring_cqe *cqe = nullptr;

  do{

    if(io_uring_peek_cqe(&state_.ring,&cqe) < 0) break;
    buffio::op_vec *vec = (buffio::op_vec *)cqe->user_data;
    ssize_t res = static_cast<ssize_t>(cqe->res);

    state_.io_count -= 1; 
    assert(state_.io_count >= 0);

    buffio::CoroutineHandle task = {};

    switch(vec->index()){
       case 0:
          return;
        break;
      case 1:
      case 2:
        std::get<1>(*vec)->rval = res;
        task = std::get<1>(*vec)->task;
       break;
      case 3:
      case 4:
        std::get<4>(*vec)->rval = res;
        task = std::get<4>(*vec)->task;
      break;
      case 5: /* no op*/
       // std::get<5>(*vec)->rval = res;
      default:
        return;
      break;
    };

     
    io_uring_cqe_seen(&state_.ring,cqe);
    task_queue.enqueue(task);
 
    cycle -= 1;
  }while(cycle > 0);
};

int buffio::Worker::run(){

 assert(!task_queue.empty());

 constexpr int event_max = 1024;
 
 std::optional<buffio::CoroutineHandle> task;

 bool exit = false;
 int epoll_fd = state_.fd;
 int task_exec_cycle = 64;
 int timeout = 0;

 struct epoll_event ev;
 ev.events = EPOLLIN;
 ev.data.fd = state_.ring.ring_fd;
 
 if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,state_.ring.ring_fd,&ev) < 0)
       return -1;
 /* insert code to dequeue expired timer */
 /* end */

 struct epoll_event event[event_max];


 do{

    
   int error_epoll = epoll_wait(epoll_fd,event,event_max,timeout);
   if(error_epoll < 0 && errno != EINTR){ 
       std::cout<<"[epoll error] "<<strerror(errno)<<std::endl;
     return -1;
   }

   for(int idx = 0; idx < error_epoll ; idx++){

     struct epoll_event *evnt = (event + idx); 
     if(evnt->data.fd == state_.ring.ring_fd){
         consume_done();
         continue;
     };
     

     /*
        handle socket read write
        buffio::socket *socket_ = (buffio::socket *)evnt->data.ptr;

     */
   };
   
   do{
      
     task = task_queue.dequeue();
     if(!task){ 
       break;
     }

     task->resume();
     task_exec_cycle -= 1;

   }while(task_exec_cycle > 0);

   flush();

   task_exec_cycle = 100;
   exit = task_queue.empty() && state_.io_count <= 0 ? true : false;
   timeout = task_queue.empty() ? -1 : 0;

 }while(!exit);

 return 0;
};

bool buffio::Worker::push(buffio::op_vec &vec) {
  
 if (!state_.submit_queue.enqueue(vec)) return false;

 if (state_.submit_queue.count() > 10)
        this->flush();

  return true;
};
