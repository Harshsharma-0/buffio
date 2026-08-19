#ifndef BUFFIO_WORKER
#define BUFFIO_WORKER

#include "buffio/defs.hpp"
#include "buffio/macro.hpp"
#include "buffio/lfqueue.hpp"

#include <atomic>
#include <latch>

#if defined(BUFFIO_OS_LINUX) || defined(BUFFIO_OS_BSD)

#include <pthread.h>
#include <semaphore.h>

#elif defined(BUFFIO_OS_WINDOWS)
#include <windows.h>
#include <processthreadsapi.h>
#include <errhandlingapi.h>
#include <synchapi.h>
#elif 
 #error unsupported os
#endif


#ifndef BUFFIO_WORKER_QUEUE_ORDER
 #define BUFFIO_WORKER_QUEUE_ORDER 4
#endif

#define BUFFIO_MAX_WORKER 16
#define BUFFIO_SLEEP_QUEUE_ORDER 4

using bfThreadMain =  BUFFIO_OS_INSERT(pthread_t,pthread_t,HANDLE);
using bfThreadQueue = buffio::lfQueue<BGLOBOPVEC,buffio::lfMemMode::stack,BUFFIO_WORKER_QUEUE_ORDER>; 
using bfThreadSem = BUFFIO_OS_INSERT(sem_t,sem_t,HANDLE);
using bfThreadSleepingQueue = buffio::lfQueue<BUFFIO_OS_INSERT(bfThreadSem*,bfThreadSem,bfThreadSem)>;

namespace buffio{
class worker{



  public:
    BUFFIO_CLASS_PROTECT(worker);
    int init(int numWorker);
    void pushWork(BGLOBOPVEC vec){}
    buffio::vTask pullResume(){return nullptr;}
    ~worker();
     worker();
  private:

  int workerNum;
  std::atomic<int> sleeping;
  std::atomic<int> active;
  std::atomic<int> cntrl;
  void *workers;
  bfThreadSleepingQueue  sleepingQueue;
  bfThreadQueue queue;
  bfThreadQueue doneQueue;

};
};
#endif
