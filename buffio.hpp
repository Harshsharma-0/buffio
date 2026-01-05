#ifndef BUFF_IO
#define BUFF_IO

#include "./buffiolog.hpp"
#include "./buffioenum.hpp"

#include <coroutine>

#if defined(BUFFIO_IMPLEMENTATION)


#include "./buffiopromise.hpp"



#include "./buffiosock.hpp"
#include "./buffioqueue.hpp"
#include "./buffiothread.hpp"
#include "./buffiolfqueue.hpp"
#include "./buffiosockbroker.hpp"
#include "./buffioschedular.hpp"

#endif
#endif

/*
  static int eventloop(void *data) {
   
    while (queue->empty() == false) {
      buffiotaskinfo *taskinfo = queue->pop();
      if(taskinfo == nullptr) break;

      buffioroutine taskhandle = taskinfo->task;
      buffiopromise *promise = &taskhandle.promise();

      // executing task only when status is executing to avoid error;
      if (promise->selfstatus.status == BUFFIO_ROUTINE_STATUS_EXECUTING) {
        taskhandle.resume();
      }
     
      switch (taskhandle.promise().selfstatus.status) {
        /* This case is true when the task want to push some
         * task to the queue and reschedule the current task
         * we can also push task buy passing the eventloop
         * instance to the task and the push from there
         * and must be done via eventloop instance that directly
         * associating with queue;
         *
      case BUFFIO_ROUTINE_STATUS_PAUSED:{
          buffiotaskinfo *new_ = pqueue->pushandget();
          new_->id = buffioinstance::genid(instance->idfactor);
          new_->task = promise->pushhandle;

        queue->push(new_);
        instance->idfactor += 1;
        /* This case is true when the task want to give control
         * back to the eventloop and we reschedule the task to
         * execute the next task in the queue.
         *
         *
      }
      case BUFFIO_ROUTINE_STATUS_YIELD:
        promise->selfstatus.status = BUFFIO_ROUTINE_STATUS_EXECUTING;
        queue->push(taskinfo);
        break;

        /*  This case is true when the task wants to
         *  wait for certain other operations;
         *

      case BUFFIO_ROUTINE_STATUS_WAITING:{
          buffiotaskinfo *info = pqueue->pushandget();
          info->id = buffioinstance::genid(instance->idfactor);
          info->task = promise->waitingfor;
          queue->push(info);
          waitingtask.push(info->id,taskinfo);
          instance->idfactor += 1;
        }
        break;
        /* These cases follow the same handling process -
         *
         *  1) BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
         *  2) BUFFIO_ROUTINE_STATUS_ERROR:
         *  3) BUFFIO_ROUTINE_STATUS_DONE:
         *
         *
         *  
        case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
        case BUFFIO_ROUTINE_STATUS_ERROR:
        case BUFFIO_ROUTINE_STATUS_DONE:

        // pulling out any task awaiter in
        auto *handle = waitingtask.pop(taskinfo->id);

        if (handle != nullptr) {
          // setting the waiter status to executing;

          handle->task.promise().selfstatus.status =
              BUFFIO_ROUTINE_STATUS_EXECUTING;
  
      
          handle->task.promise().childstatus 
                        = taskhandle.promise().selfstatus;
          queue->push(handle);
        }


        // destroying task handle if task errored out or task done
       
       taskhandle.destroy();
       pqueue->popget((void *)taskinfo); 
       total += 1;
        /*
         * removing the task from execqueue and putting in freequeuelist
         * to reuse the task allocated chunk later and prevent allocation
         * of newer chunk every time.
         */
/*
        break;
      };
    };
    BUFFIO_INFO(" Queue empty: no task to execute ,total : ",total);
    return 0;
  }

  void fireeventloop(enum BUFFIO_EVENTLOOP_TYPE eventlooptype) {
    ieventlooptype = eventlooptype;
    switch (eventlooptype) {
    case BUFFIO_EVENTLOOP_SYNC:
      eventloop(this);
      break;
    case BUFFIO_EVENTLOOP_ASYNC:
      break;
    }
    return;
  };

  buffioinstance() : ieventlooptype(BUFFIO_EVENTLOOP_DOWN) , idfactor(0){ 
    pqueue.setdefault({0});
    equeue.setdefault(nullptr);
    equeue.reserve(25);
    pqueue.reserve(25);
  ~buffioinstance() {
    BUFFIO_INFO("Total executed task : ", equeue.queuen());
    // clean up-code;
  };

  int push(buffioroutine routine){
    switch (ieventlooptype) {
    case BUFFIO_EVENTLOOP_DOWN:
    case BUFFIO_EVENTLOOP_SYNC:{
      buffiotaskinfo *fresh = pqueue.pushandget();
      fresh->task = routine;
      fresh->id = buffioinstance::genid(idfactor);
      equeue.push(fresh);
      idfactor += 1;
      }
      break;
    case BUFFIO_EVENTLOOP_ASYNC:
      break;
    }
    return 0;
  };
*/

