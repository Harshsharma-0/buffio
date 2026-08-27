#ifndef BUFFIO_THREAD
#define BUFFIO_THREAD

#include "buffio/config.hpp"
#include "buffio/macro.hpp"

#if defined(BUFFIO_OS_LINUX) || defined(BUFFIO_OS_BSD)
 #include <pthread.h>
 #include <semaphore.h>
#endif

namespace buffio{

 using threadFuncSig = void(*)(void *);
 
 class thread{
  public:

   thread():routine(nullptr),args(nullptr){};
  
   int run(threadFuncSig start,void *args);
   int join();
  private:
   BUFFIO_OS_INSERT(pthread_t,pthread_t,HANDLE) handle;
   threadFuncSig routine;
   void *args;
 };

 class semaphore{
  public:

      
    semaphore& operator=(semaphore const &val){return *this;}
    semaphore(){};
    int create(size_t initialValue){
      BUFFIO_LIN_INSERT(if(sem_init(&lsem,0,initialValue) == 0) return 0;)

      BUFFIO_WIN_INSERT( 
       LONG lmaxCount = static_cast<LONG>(initialValue);
       HANDLE semHandle = createSemaphoreA(NULL,lmaxCount,lmaxCount,NULL);
       lsem = semHandle;
       if(semHandle != NULL) return 0;
     )
  /* HERE ONLY IF THERE IS ERROR CREATING SEMAPHORE */

  return -1;

    };
    int post(){
     BUFFIO_LIN_INSERT(sem_post(&lsem);)
     BUFFIO_WIN_INSERT(ReleaseSemaphore(lsem,1));
     return 0;
    };

    int wait(){
     BUFFIO_LIN_INSERT(sem_wait(&lsem);)
     BUFFIO_WIN_INSERT(waitForSingleObject(lsem,-1));
     return 0;
    };
    void destroy(){
      BUFFIO_LIN_INSERT(sem_destroy(&lsem);)
      BUFFIO_WIN_INSERT(closeHandle(lsem);)
    }; 
  private:
    BUFFIO_OS_INSERT(sem_t,sem_t,HANDLE) lsem;
 };

};
#endif
