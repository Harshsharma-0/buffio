#include "buffio/worker.hpp"

#define BUFFIO_WORKER_ABORT -100
/* NAMING CONVENTION CAN BE IMPROVED */

static void buffioWorkerFunc(void *args);

struct WorkerParameters{

  std::atomic<int> *psleep_count;
  std::atomic<int> *pactive_count;
  std::atomic<int> *pcontrol;
  std::latch *psync;
  WorkQueue *pwork_queue;
  WorkQueue *pcomplete_queue;
  SleepQueue  *psleep_queue;
  buffio::semaphore lock_self;
  buffio::thread thread;

};



static void buffioWorkerFunc(void *args){

  WorkerParameters state = *(WorkerParameters*)args;
  buffio::semaphore *lock_self = &((WorkerParameters*)args)->lock_self;
  buffio::noOp nullOp;

  state.pactive_count->fetch_add(1);
  state.psync->count_down();

   int exitCode = state.pcontrol->load(std::memory_order_acquire);

  while(exitCode != BUFFIO_WORKER_ABORT){

   auto val = state.pwork_queue->dequeue(&nullOp);
   BUFFIO_OP_ACTION_TABLE(val,val.index(),{

        /** code block for noOp **/
        state.psleep_count->fetch_add(1);
        state.psleep_queue->enqueue(lock_self);
        lock_self->wait(); // wait for the semephore
        state.psleep_count->fetch_add(-1);
        exitCode = state.pcontrol->load(std::memory_order_acquire);

        continue;
        break;

    });

   /* code block to try to enqueue until val it's enqueued, only here if there's work donw */
    while(!state.pcomplete_queue->enqueue(val)){
      /** code block for noOp **/
        state.psleep_count->fetch_add(1);
        state.psleep_queue->enqueue(lock_self);
        lock_self->wait(); // wait for the semephore
        state.psleep_count->fetch_add(-1);
        exitCode = state.pcontrol->load(std::memory_order_acquire);

    };

    exitCode = state.pcontrol->load(std::memory_order_acquire);
  };

   state.pactive_count->fetch_add(-1);

  return;
};


buffio::Worker::Worker(){
 state_.workers = nullptr;
 state_.sleep_count = state_.active_count = 0;
 state_.control =  0; // BUFFIO_WORKER_ABORT;
};

buffio::Worker::~Worker(){
 if(!state_.workers) return;
 
 delete []static_cast<WorkerParameters*>(state_.workers);
};

int buffio::Worker::init(int numWorker,int queueOrder){
    
  WorkerParameters *winfo = nullptr;
  WorkerParameters wparam = {};

  /* evaluating the maximum worker thread that can concurrently access the queue*/
  size_t maxWorker =  buffio::lfSpec::getSize<BUFFIO_WORKER_QUEUE_ORDER>();

  /* checking it the maxWorker exceeds the maximun supported worker */
  maxWorker = maxWorker > BUFFIO_MAX_WORKER ? maxWorker : BUFFIO_MAX_WORKER;

  /* checking numWorker for negative value */
  numWorker = numWorker <= 0 ? 4 : numWorker;

  /* if numWorker exceed maxWorker set it to max worker */
  numWorker = numWorker > maxWorker ? maxWorker : numWorker;
  

  /* initlising the sleeping Queue */ 
  if(state_.submit_queue.lfstart(BUFFIO_WORKER_QUEUE_ORDER) != 0) 
    return -1;
  if(state_.complete_queue.lfstart(BUFFIO_WORKER_QUEUE_ORDER) !=  0) 
    return -1;
  if(state_.sleep_queue.lfstart(BUFFIO_SLEEP_QUEUE_ORDER) != 0)
    return -1;
   
  winfo = new(std::nothrow) struct WorkerParameters[numWorker];

  if(winfo == nullptr){ 
    return -1; 
  };

  /* latch to ensure all resume execution only after all workers are initlised */
  std::latch sync{numWorker}; 
  int nWorker = numWorker;

  wparam = {   &state_.sleep_count,
               &state_.active_count,
               &state_.control,
               &sync,
               &state_.submit_queue,
               &state_.complete_queue,
               &state_.sleep_queue,
  };
  

  for(int i = 0; i < numWorker; i++){
   winfo[i] = wparam;

   if(winfo[i].lock_self.create(0) < 0) break;
   if(winfo[i].thread.run(buffioWorkerFunc,(void*)(winfo + i)) != 0){ 
     winfo[i].lock_self.destroy();
     break; 
   }
   nWorker -= 1;
  };

  /* checking if createThread loop failed completely*/
  if(nWorker == numWorker){ 
    delete []winfo;
    return -1; 

  };
  /* auto caliberationg workerNum if the createThread loop failed partially */
  state_.worker_count = numWorker - nWorker;

  while(nWorker != 0){
    /* decrements the sync counter to wait for worker to be executing */
       sync.count_down();
       nWorker -= 1;
  }

  /* checking and waiting for the worker to be ready for work */
  if(!sync.try_wait()) sync.wait();
  state_.workers = static_cast<void*>(winfo);
   
  return 0; 
};

void buffio::Worker::pushWork(BGLOBOPVEC vec){  };

