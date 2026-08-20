#include "buffio/file.hpp"
#include "buffio/worker.hpp"
#include "buffio/lfqueue.hpp"

buffio::Worker::Worker(){}
buffio::Worker::~Worker(){}

int buffio::Worker::init(int numWorker){
 return this->init(numWorker,BUFFIO_WORKER_QUEUE_ORDER);
};

int buffio::Worker::init(int numWorker,int queueOrder){   
   size_t size =  buffio::lfSpec::get_size(static_cast<size_t>(queueOrder));
   if(size == 0) return -1;
   if(io_uring_queue_init(size,&state_.ring,0) != 0)
     return -1;

  return 0;
};

bool buffio::Worker::flush(){

  struct io_uring *ring = &state_.ring;
  buffio::noOp noop;
  bool exit = false;
  int total = 0;

  while(!exit){
    
    buffio::op_vec op = state_.submit_queue.dequeue(&noop);
    BUFFIO_OP_IO_URING_TABLE(ring,op.index(),{
       /*op table empty */
        
       break; 
    });

   total += 1; 

 if(io_uring_sq_space_left(ring) == 0)
      io_uring_submit(ring); // returns the submitted number of entry
   };

  
 return true;
};

int buffio::Worker::consume(){};

bool buffio::Worker::push(buffio::op_vec vec){

  if(state_.submit_queue.enqueue(vec)) return true;
  this->flush();   
  
  return true;
};
