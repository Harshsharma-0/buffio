#ifndef BUFFIO_SCHEDULAR
#define BUFFIO_SCHEDULAR

/*
 * Error codes range reserved for buffioschedular
 *  [4000 - 5500]
 *  4000 <= errorcode <= 5500
 */

#if !defined(BUFFIO_IMPLEMENTATION)
#include "buffiopromsie.hpp"
#include "buffiosockbroker.hpp"
#endif

#include "buffioQueue.hpp"
#include <cassert>

class buffioScheduler {


public:
  // constructor overload.
  buffioScheduler() = default;
  ~buffioScheduler() = default;

  using memRequest = buffioMemoryPool<char[sizeof(BUFFIO_REQUEST_MAX_SIZE)]>;
  int run() {

    if(queue.empty() && timerClock.empty()) return -1;

    memRequest reqMemoryPool;
    buffioThread threadPool;

    int error = 0;

    if ((error = poller.start(threadPool)) != 0) return error;
    if ((error = reqMemoryPool.init()) != 0) return error;
    if ((error = syncPipe.pipe()) != 0) return error;
    
    auto pipe = syncPipe.get();
    if ((error = poller.pollOp(pipe->fd.pipeFd[0], EPOLLIN, nullptr)) != 0)
      return error;

    struct epoll_event evnt[1024];
    bool exit = false;
    int timeout = 0;
    while (exit != true) {
      timeout = getWakeTime(&exit);
      if(exit == true) break;

      int nfd = poller.poll(evnt, 1024,timeout);
      if(nfd < 0) break;
      if(nfd != 0) 
         processEvents(evnt,nfd);

      int error = yieldQueue(10);
    };
   threadPool.threadfree();
    return 0;
  };
   int processEvents(struct epoll_event evnts[], int len){
    return 0;
   };
   int getWakeTime(bool *flag){
    uint64_t startTime = timerClock.now();
    int looptime = timerClock.getNext(startTime); 
    while(looptime == 0){
          auto clkWork = timerClock.get();
          auto *promise = getPromise<char>(clkWork);
          promise->setStatus(buffioRoutineStatus::executing);
          queue.push(clkWork);
          timerClock.pop();
          looptime = timerClock.getNext(startTime);
       };
      if(!queue.empty()) return 0;
      if(queue.empty() && timerClock.empty() && !poller.busy()){
          *flag = true;
          return 0;
      };
      return looptime;
   };

   int yieldQueue(int chunk){
     if(queue.empty()) return -1;

    for(int i = chunk; 0 < i; i--){
       auto handle = queue.get();
       auto promise = getPromise<char>(handle->current); 
       if(promise->checkStatus() == buffioRoutineStatus::executing)
           handle->current.resume();

       switch (promise->checkStatus()) {
       case buffioRoutineStatus::executing:
         break;
       case buffioRoutineStatus::yield: {
          promise->setStatus(buffioRoutineStatus::executing);
        } break;
       case buffioRoutineStatus::waitingFd:{
        } break;
       case buffioRoutineStatus::paused: {
        } break;
       case buffioRoutineStatus::waiting: {
          queue.push(promise->getChild(),handle);
          queue.erase();
        } break;
       case buffioRoutineStatus::waitingTimer:{
          handle->current = nullptr;
          queue.pop();
        }break;
       case buffioRoutineStatus::pushTask: {
        } break;
       case buffioRoutineStatus::unhandledException:
        [[fallthrough]];

       case buffioRoutineStatus::error: 
        [[fallthrough]];

       case buffioRoutineStatus::wakeParent:{
          if(handle->waiter != nullptr){
              auto promiseP = getPromise<char>(handle->waiter->current);
              promiseP->setStatus(buffioRoutineStatus::executing);    
              queue.push(handle->waiter);
              promise->setStatus(buffioRoutineStatus::paused);
             break;
          };
        }

        [[fallthrough]];

       case buffioRoutineStatus::zombie:
        [[fallthrough]];

       case buffioRoutineStatus::done:{
          handle->current.destroy();
          handle->current = nullptr;
          queue.pop();
          break;
        }
       };
        if(queue.empty())break;
         queue.mvNext();
    };
     return 0;
   };

   int push(buffioPromiseHandle handle){
    auto *promise = getPromise<char>(handle);
    promise->setInstance(&timerClock,&poller);
    return queue.push(handle);
  };
private:
  buffioFd syncPipe;
  buffioSockBroker poller;
  buffioClock timerClock;
  buffioTaskQueue <>queue;
};

#endif
