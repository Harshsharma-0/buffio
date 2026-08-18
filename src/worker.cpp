#include "buffio/lfqueue.hpp"
#include "buffio/worker.hpp"
#include "buffio/file.hpp"
#include <pthread.h>
#include <semaphore.h>
#include <atomic>
#include <latch>

#define thread_return_type BUFFIO_OS_INSERT(void *,void*,DWORD WINAPI)   
#define thread_arg_type BUFFIO_OS_INSERT(void *,void*,LPVOID) args 

#define BUFFIO_WORKER_ABORT -100

using buffioThreadMainType =  BUFFIO_OS_INSERT(pthread_t,pthread_t,HANDLE);
using buffioThreadQueueType = buffio::lfQueue<BGLOBOPVEC,buffio::lfMemMode::stack,4>; 
using buffioThreadSemType = BUFFIO_OS_INSERT(sem_t,sem_t,HANDLE);
static thread_return_type buffioWorkerFunc(thread_arg_type);

typedef struct workerFuncInfo{

  std::atomic<int> *sleeping;
  std::atomic<int> *active;
  std::atomic<int> *cntrl;
  std::latch *sync;
  buffioThreadQueueType *queue;
  buffioThreadSemType *sem;
  buffioThreadMainType threadMain;

}workerFuncInfo, *pWorkerFuncInfo;

typedef struct workerControlField{

  int workerNum;
  std::atomic<int> sleeping;
  std::atomic<int> active;
  std::atomic<int> cntrl;
  pWorkerFuncInfo workers;
  buffioThreadQueueType queue;
  buffioThreadSemType sem;

}workerControlFields,*pWorkerControlFields;


#if defined(BUFFIO_OS_LINUX) || defined(BUFFIO_OS_BSD)

static int create_thread(pWorkerFuncInfo info,void *args){
 pthread_attr_t attribute;
 pthread_attr_init(&attribute);
 size_t stackSize = 5 * 1024 *1024;
 pthread_attr_setstacksize(&attribute,stackSize);

 if(pthread_create(&info->threadMain,&attribute,buffioWorkerFunc,args) != 0)
    return -1;

 pthread_attr_destroy(&attribute);
 return 0;
};

#elif defined(BUFFIO_OS_WINDOW)
#include <windows.h>


#else
 #error not supporitng this os
#endif

static thread_return_type buffioWorkerFunc(thread_arg_type){
  pWorkerFuncInfo syncFields = (pWorkerFuncInfo)args;
  
  struct{
    std::latch *sync;  
    std::atomic<int> *active;
    std::atomic<int> *sleeping;
    std::atomic<int> *cntrl;
    buffioThreadQueueType *queue;
    buffioThreadSemType *sem;
    BGLOBOPVEC noOp;
  }local;

  buffio::noOp nullOp;

  local = {
           syncFields->sync,
           syncFields->active,
           syncFields->sleeping,
           syncFields->cntrl,
           syncFields->queue,
           syncFields->sem,
           &nullOp
          };

  local.active++;
  local.sync->count_down();

  while(local.cntrl->load(std::memory_order_acquire) != BUFFIO_WORKER_ABORT){
   if(local.queue->empty()){
     local.sleeping++;
     /* CODE TO WAIT FOR WORK*/
   };
 
   auto val = local.queue->dequeue(local.noOp);
   BUFFIO_OP_ACTION_TABLE(val,val.index(),{
        /** INSERT CODE FOR DEFAULT FIELD **/
       std::cout<<"[no op]"<<std::endl;
        break;
       }); 
  };
  std::cout<<"[hello world]"<<std::endl;
  return BUFFIO_OS_INSERT(nullptr,nullptr,0);
};


buffio::worker::worker(){
  workerControl = nullptr;
};

buffio::worker::~worker(){
 if(!workerControl) return;
 
 pWorkerControlFields cinfo = (pWorkerControlFields)workerControl;
 delete []cinfo->workers;
 delete cinfo;
};

int buffio::worker::init(int numWorker){

 pWorkerControlFields cinfo = new(std::nothrow)workerControlFields;
 if(cinfo == nullptr) return -1;
 
 numWorker = numWorker <= 0 ? 4 : numWorker;
 pWorkerFuncInfo winfo = new(std::nothrow) struct workerFuncInfo[numWorker];



 if(winfo == nullptr){ 
   delete cinfo;
   return -1; 
 };
 
  cinfo->workers = winfo;
  cinfo->sleeping = 0;
  cinfo->active = 0;
  cinfo->workerNum = 0;
  cinfo->cntrl = BUFFIO_WORKER_ABORT; 
  workerControl = (void *)cinfo; 

  std::latch sync{numWorker};
 
  int nWorker = numWorker;

  for(int i = 0; i < numWorker; i++){
   winfo[i].sync = &sync;
   winfo[i].queue = &cinfo->queue;
   winfo[i].sleeping = &cinfo->sleeping;
   winfo[i].active = &cinfo->active;
   winfo[i].cntrl = &cinfo->cntrl;
   winfo[i].sem = &cinfo->sem;
    if(create_thread((winfo + i),(void *)(winfo + i)) != 0) break;
   nWorker -= 1;
  };

  if(nWorker == numWorker) return -1;
  cinfo->workerNum = numWorker - nWorker;

  while(nWorker != 0){
       sync.count_down();
       nWorker -= 1;
  }
  
  if(!sync.try_wait()) sync.wait();

 return 0; 
};
