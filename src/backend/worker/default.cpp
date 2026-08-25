#include "buffio/worker.hpp"
#include "buffio/memory.hpp"
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#define BUFFIO_WORKER_ABORT -100


static void buffioWorkerFunc(void *args);

struct WorkerParameters{

  std::atomic<int> *psleep_count;
  std::atomic<int> *pactive_count;
  std::atomic<int> *pcontrol;
  std::latch *psync;
  WorkQueue *pwork_queue;
  WorkQueue *pcompletion_queue;
  SleepQueue  *psleeping_queue;
  buffio::semaphore lock_self;
  buffio::thread thread;

};



static void buffioWorkerFunc(void *args){

  WorkerParameters state = *(WorkerParameters*)args;
  buffio::semaphore *lock_self = &((WorkerParameters*)args)->lock_self;

  state.pactive_count->fetch_add(1);
  state.psync->count_down();

  int exitCode = state.pcontrol->load(std::memory_order_acquire);
  
  do{
    std::optional<buffio::OpState *>op = state.pwork_queue->dequeue();
   
   if(!op){
        state.psleep_count->fetch_add(1);
        state.psleeping_queue->enqueue(lock_self);
        lock_self->wait(); // wait for the semephore
        state.psleep_count->fetch_add(-1);
        exitCode = state.pcontrol->load(std::memory_order_acquire);
        continue;
   };
   
   buffio::OpState *action = *op;

   action->action({nullptr, action->data}); 
   
   /* code block to try to enqueue until val it's enqueued, only here if there's work done 
    int tries_max = 1000;
    do{
      if(state.pcompletion_queue->(val)) break;
      
      if()
    }while(1);
    */

    while(!state.pcompletion_queue->enqueue(action)){

        state.psleep_count->fetch_add(1);
        state.psleeping_queue->enqueue(lock_self);
        lock_self->wait(); // wait for the semephore
        state.psleep_count->fetch_add(-1);

    };
    /* insert code to signal back that work is done, or notify the main loop */
    exitCode = state.pcontrol->load(std::memory_order_acquire);

  }while(exitCode != BUFFIO_WORKER_ABORT); 

  state.pactive_count->fetch_add(-1);

  return;
};



buffio::Worker::~Worker(){

 if(state.event.epoll_fd >= 0) close(state.event.epoll_fd);
 if(state.event.event_fd >= 0) close(state.event.event_fd);

 if(!state.workers) return;
 
 delete []static_cast<WorkerParameters*>(state.workers);
};

int buffio::Worker::init(int numWorker){
 return this->init(numWorker,(1U << BUFFIO_WORKER_QUEUE_ORDER));
};

int buffio::Worker::init_task_queues(unsigned int order){
  /* initlising the sleeping Queue */ 
  if(!state.task_queue.init())
    return -1;
  if(state.io.submit_queue.lfstart(order) != 0) 
    return -1;
  if(state.io.completed.lfstart(order) !=  0) 
    return -1;
  if(state.event.sleeping_queue.lfstart(BUFFIO_SLEEP_QUEUE_ORDER) != 0)
    return -1;

  return 0;
};

int buffio::Worker::init_worker_threads(int num){

  WorkerParameters *winfo = nullptr;
  WorkerParameters wparam = {};
  winfo = new(std::nothrow) struct WorkerParameters[num];

  if(winfo == nullptr){ 
    return -1; 
  };

  /* latch to ensure all resume execution only after all workers are initlised */
  std::latch sync{num}; 
  int nWorker = num;

  wparam = {   &state.sleep_count,
               &state.active_count,
               &state.control,
               &sync,
               &state.io.submit_queue,
               &state.io.completed,
               &state.event.sleeping_queue,
  };
  

  for(int i = 0; i < num; i++){
   winfo[i] = wparam;

   if(winfo[i].lock_self.create(0) < 0) break;
   if(winfo[i].thread.run(buffioWorkerFunc,(void*)(winfo + i)) != 0){ 
     winfo[i].lock_self.destroy();
     break; 
   }
   nWorker -= 1;
  };

  /* checking if createThread loop failed completely*/
  if(nWorker == num){ 
    delete []winfo;
    return -1; 

  };
  /* auto caliberationg workerNum if the createThread loop failed partially */
  state.worker_count = num - nWorker;

  while(nWorker != 0){
    /* decrements the sync counter to wait for worker to be executing */
       sync.count_down();
       nWorker -= 1;
  }

  /* checking and waiting for the worker to be ready for work */
  if(!sync.try_wait()) sync.wait();
  state.workers = static_cast<void*>(winfo);

  return 0;
};

int buffio::Worker::init(int numWorker,unsigned int queueSize){
    

  /* evaluating the maximum worker thread that can concurrently access the queue*/
  auto[maxWorker,order] =  buffio::utility::get_pow2(queueSize);

  /* checking it the maxWorker exceeds the maximun supported worker */
  maxWorker = maxWorker > BUFFIO_MAX_WORKER ? maxWorker : BUFFIO_MAX_WORKER;

  /* checking numWorker for negative value */
  numWorker = numWorker <= 0 ? 4 : numWorker;

  /* if numWorker exceed maxWorker set it to max worker */
  numWorker = numWorker > maxWorker ? maxWorker : numWorker;
  
  if(init_task_queues(order) != 0) return -1;
  if(init_worker_threads(numWorker) != 0) return -1; 
  if(init_epoll_event() != 0) return -1; 

  return 0; 
};

int buffio::Worker::init_epoll_event(){

  int epfd = epoll_create1(EPOLL_CLOEXEC);
  if(epfd < 0) return -1;

  int evntfd = eventfd(0,EFD_CLOEXEC | EFD_NONBLOCK);
  if(evntfd < 0) {
    close(epfd);
    return -1;
  };

  struct epoll_event evnt;
  evnt.events = EPOLLIN | EPOLLET;
  evnt.data.fd = evntfd;

  if(epoll_ctl(epfd,EPOLL_CTL_ADD,evntfd,&evnt) < 0){
    close(epfd);
    close(evntfd);
    return -1;
  };

  state.event.epoll_fd = epfd;
  state.event.event_fd = evntfd;
  return 0;
};

int buffio::Worker::run(){
 
  while(!should_exit()){
    wait_event();
    run_tasks(64);
    flush_io_requests();
  };
  return 0;
};

bool buffio::Worker::should_exit(){
  return state.task_queue.empty() && state.io.pending == 0 ? true : false;
};

int buffio::Worker::wait_event(){ 

  int epoll_fd = state.event.epoll_fd;
  int event_fd = state.event.event_fd;
  constexpr int event_size = 1024;
  struct epoll_event events[1024];

  int count = epoll_wait(epoll_fd,events,event_size);
  if(count < 0 && errno != EINTR)
      return -1;

  struct epoll_event *evnt = nullptr;
  for(int i = 0; i < count ; i++){
    evnt = (events + i);
    if(evnt->data.fd == event_fd){
       flush_io_comlepted();
    };
    
    buffio::OpState *op = static_cast<buffio::OpState*>(evnt->data.ptr);

   /* perform socket read/write */

  };
 
  return 0; 
};

int buffio::Worker::run_tasks(unsigned int budget){ 
  while(budget--){
   std::optional<buffio::CoroutineHandle> task = state.task_queue.dequeue();
   if(!task) break;
   task->resume();
  };
  return 0; 
};

bool buffio::Worker::flush(){ return true;};
int buffio::Worker::flush_io_completed(){ return 0;}
int buffio::Worker::flush_io_requests(){ return 0; }
bool buffio::Worker::push(buffio::OpState &vec){ return true; };

