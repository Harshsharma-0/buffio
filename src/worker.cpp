#include "buffio/worker.hpp"
#include "buffio/file.hpp"

#define thread_return_type BUFFIO_OS_INSERT(void *,void*,DWORD WINAPI)   
#define thread_arg_type BUFFIO_OS_INSERT(void *,void*,LPVOID) args 

#define BUFFIO_WORKER_ABORT -100
/* NAMING CONVENTION CAN BE IMPROVED */

static thread_return_type buffioWorkerFunc(thread_arg_type);

typedef struct workerParameters{

  std::atomic<int> *pSleeping;
  std::atomic<int> *pActive;
  std::atomic<int> *pCntrl;
  std::latch *pSync;
  bfThreadQueue *pQueue;
  bfThreadQueue *pDoneQueue;
  bfThreadSleepingQueue  *pSleepingQueue;
  bfThreadSem lockSelf;
  bfThreadMain threadMain;
}workerParameters, *pWorkerParameters;


static int buffioCreateThread(bfThreadMain *pId,void *args){

  size_t stackSize = 5 * 1024 *1024;

#if defined(BUFFIO_OS_LINUX) || defined(BUFFIO_OS_BSD)

  pthread_attr_t attribute;
  pthread_attr_init(&attribute);
  pthread_attr_setstacksize(&attribute,stackSize);
  int error = pthread_create(pId,&attribute,buffioWorkerFunc,args); 

#elif defined(BUFFIO_OS_WINDOW)
   HANDLE threadHandle = createThread(NULL,stackSize,buffioWorkerFunc,args,STACK_SIZE_PARAM_IS_A_RESERVATION,NULL);
#else

  #error unsupported platform

#endif

 if(BUFFIO_OS_INSERT(error,error,threadHandle) != BUFFIO_OS_INSERT(0,0,NULL)){
   /* return BENOMEM from here */
   if(BUFFIO_OS_INSERT(EAGAIN,EAGAIN,ERROR_NOT_ENOUGH_MEMORY) == BUFFIO_OS_INSERT(errno,errno,GetLastError())){
     return -1;
   };
   /* return BEUNKNOWM from here */
  return -1;
 }

 /* HERE ONLY IF SUCCESS */
 BUFFIO_WIN_INSERT(*pId = threadHandle);
 BUFFIO_LIN_INSERT(pthread_attr_destroy(&attribute));
 return 0;
};

static int buffioCreateSem(bfThreadSem *sem,size_t maxCount) {

  BUFFIO_LIN_INSERT(if(sem_init(sem,0,maxCount) == 0) return 0;)

  BUFFIO_WIN_INSERT( 
   LONG lmaxCount = static_cast<LONG>(maxCount);
   HANDLE semHandle = createSemaphoreA(NULL,lmaxCount,lmaxCount,NULL);
   if(semHandle != NULL) return 0;
  )
  /* HERE ONLY IF THERE IS ERROR CREATING SEMAPHORE */

  return -1;
};



static inline int buffioSemWait(bfThreadSem *sem){
  BUFFIO_LIN_INSERT(sem_wait(sem);)
  BUFFIO_WIN_INSERT(waitForSingleObject(*sem,-1));
  return 0;
};
static inline int buffioSemPost(bfThreadSem *sem){
  BUFFIO_LIN_INSERT(sem_post(sem);)
  BUFFIO_WIN_INSERT(ReleaseSemaphore(*sem,1));
  return 0;
};
static inline void buffioSemDestroy(bfThreadSem *sem){
    BUFFIO_OS_INSERT(
          sem_destroy(sem),
          sem_destroy(sem),
          closeHandle(*sem)
          );

}

static thread_return_type buffioWorkerFunc(thread_arg_type){
  pWorkerParameters syncFields = (pWorkerParameters)args;
  bfThreadSem sleepSem;
  buffio::noOp nullOp;
 
  struct{
    std::latch *pSync;  
    std::atomic<int> *pActive;
    std::atomic<int> *pSleeping;
    std::atomic<int> *pCntrl;
    bfThreadQueue *pQueue;
    bfThreadQueue *pDoneQueue;
    bfThreadSleepingQueue *pSleepingQueue;
    bfThreadSem *lockSelf;
    BGLOBOPVEC noOp;
  }local = {
           syncFields->pSync,
           syncFields->pActive,
           syncFields->pSleeping,
           syncFields->pCntrl,
           syncFields->pQueue,
           syncFields->pDoneQueue,
           syncFields->pSleepingQueue,
           &syncFields->lockSelf,
           &nullOp
          };

  local.pActive->fetch_add(1);
  local.pSync->count_down();

   int exitCode = local.pCntrl->load(std::memory_order_acquire);

  while(exitCode != BUFFIO_WORKER_ABORT){

   auto val = local.pQueue->dequeue(local.noOp);
   BUFFIO_OP_ACTION_TABLE(val,val.index(),{

        /** code block for noOp **/
        local.pSleeping->fetch_add(1);
        local.pSleepingQueue->enqueue(local.lockSelf);
        buffioSemWait(local.lockSelf); // wait for the semephore
        local.pSleeping->fetch_add(-1);
        break;

    });

   /* code block to try to enqueue until val it's enqueued */
    while(!local.pDoneQueue->enqueue(val)){
       local.pSleeping->fetch_add(1);
       local.pSleepingQueue->enqueue(local.lockSelf);
       buffioSemWait(local.lockSelf); // wait for the semephore
       local.pSleeping->fetch_add(-1);
    };

    exitCode = local.pCntrl->load(std::memory_order_acquire);
  };

   local.pActive->fetch_add(-1);

  return BUFFIO_OS_INSERT(nullptr,nullptr,0);
};


buffio::worker::worker(){
  workers = nullptr;
  sleeping = active = 0;
  cntrl =  0; // BUFFIO_WORKER_ABORT;
;
};

buffio::worker::~worker(){
 if(!workers) return;
 
 delete []static_cast<pWorkerParameters>(workers);
};

int buffio::worker::init(int numWorker){
    
  pWorkerParameters winfo = nullptr;
  workerParameters wparam = {};

  /* evaluating the maximum worker thread that can concurrently access the queue*/
  size_t maxWorker =  buffio::lfSpec::getSize<BUFFIO_WORKER_QUEUE_ORDER>();

  /* checking it the maxWorker exceeds the maximun supported worker */
  maxWorker = maxWorker > BUFFIO_MAX_WORKER ? maxWorker : BUFFIO_MAX_WORKER;

  /* checking numWorker for negative value */
  numWorker = numWorker <= 0 ? 4 : numWorker;

  /* if numWorker exceed maxWorker set it to max worker */
  numWorker = numWorker > maxWorker ? maxWorker : numWorker;
  
  /* initlising the sleeping Queue */ 
  if(sleepingQueue.lfstart(BUFFIO_SLEEP_QUEUE_ORDER) != 0) return -1;
  
  winfo = new(std::nothrow) struct workerParameters[numWorker];

  if(winfo == nullptr){ 
    return -1; 
  };


  std::latch sync{numWorker}; 
  int nWorker = numWorker;

  wparam = {   &sleeping,
               &active,
               &cntrl,
               &sync,
               &queue,
               &doneQueue,
               &sleepingQueue
  };


  for(int i = 0; i < numWorker; i++){
   winfo[i] = wparam;
   if(buffioCreateSem(&winfo[i].lockSelf,0) < 0) break;
   if(buffioCreateThread(&winfo[i].threadMain,(void *)(winfo + i)) != 0){ 
     buffioSemDestroy(&winfo[i].lockSelf);
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
  workerNum = numWorker - nWorker;

  while(nWorker != 0){
    /* decrements the sync counter to wait for worker to be executing */
       sync.count_down();
       nWorker -= 1;
  }

  /* checking and waiting for the worker to be ready for work */
  if(!sync.try_wait()) sync.wait();
  workers = static_cast<void*>(winfo);
   
  return 0; 
};
