#ifndef BUFF_IO
#define BUFF_IO

#include "./buffiolog.hpp"

#include <coroutine>


enum BUFFIO_ROUTINE_STATUS {
  BUFFIO_ROUTINE_STATUS_WAITING = 21,
  BUFFIO_ROUTINE_STATUS_EXECUTING,
  BUFFIO_ROUTINE_STATUS_YIELD,
  BUFFIO_ROUTINE_STATUS_ERROR,
  BUFFIO_ROUTINE_STATUS_PAUSED,
  BUFFIO_ROUTINE_STATUS_DONE,
  BUFFIO_ROUTINE_STATUS_EHANDLER,
  BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION,
  BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION_DONE,
};

enum BUFFIO_TASK_STATUS {
  BUFFIO_TASK_SWAPPED = 31,
  BUFFIO_TASK_WAITER_EXCEPTION_WAITING,
  BUFFIO_TASK_WAITER_EXCEPTION_DONE,
  BUFFIO_TASK_WAITER_NONE,
};

enum BUFFIO_QUEUE_STATUS {
  BUFFIO_QUEUE_STATUS_ERROR = 40,
  BUFFIO_QUEUE_STATUS_SUCCESS = 41,
  BUFFIO_QUEUE_STATUS_YIELD,
  BUFFIO_QUEUE_STATUS_EMPTY,
  BUFFIO_QUEUE_STATUS_SHUTDOWN,
  BUFFIO_QUEUE_STATUS_CONTINUE,
};

enum BUFFIO_EVENTLOOP_TYPE {
  BUFFIO_EVENTLOOP_SYNC = 50, // use this to block main thread
  BUFFIO_EVENTLOOP_ASYNC,     // use this to launch a thread;
 // BUFFIO_EVENTLOOP_SEPERATE,  // use this to create a seperate process from main
  BUFFIO_EVENTLOOP_DOWN,      // indicates eventloop is not running

};

enum BUFFIO_ACCEPT_STATUS {
  BUFFIO_ACCEPT_STATUS_ERROR = 60,
  BUFFIO_ACCEPT_STATUS_SUCCESS = 61,
  BUFFIO_ACCEPT_STATUS_NA,
  BUFFFIO_ACCEPT_STATUS_NO_HANDLER,
};

#if defined(BUFFIO_IMPLEMENTATION)

struct buffioinfo {
  const char *address;
  int portnumber;
  int listenbacklog;
  int socktype;
  int sockfamily;
};

struct buffiobuffer {
  char *data;
  size_t filled;
  size_t size;
};

struct clientinfo {
  const char *address;
  int clientfd;
  int portnumber;
  buffiobuffer readbuffer;
  buffiobuffer writebuffer;
};

#include "./buffiopromise.hpp"

struct buffiotaskinfo {
  buffioroutine handle;
  struct buffiotaskinfo *next;
  struct buffiotaskinfo *prev;
};

#include "./buffiosock.hpp"
#include "./buffioqueue.hpp"
#include "./buffiothread.hpp"
#include "./buffiolfqueue.hpp"
#include "./buffiosockbroker.hpp"

class buffioinstance {

public:
  static int eventloop(void *data) {
    buffioinstance *instance = (buffioinstance *)data;
    buffioqueue<buffiotaskinfo, buffioroutine> *queue = &instance->iqueue;
    
    buffiosockbroker iobroker(BUFFIO_EPOLL_MAX_THRESHOLD); 

    while (queue->empty() == false) {
      buffiotaskinfo *taskinfo = queue->getnextwork();
      if (taskinfo == nullptr)
        break;
      buffioroutine taskhandle = taskinfo->handle;
      buffiopromise *promise = &taskhandle.promise();

      // executing task only when status is executing to avoid error;
      if (promise->selfstatus.status == BUFFIO_ROUTINE_STATUS_EXECUTING) {
        taskhandle.resume();
      }

      switch (taskhandle.promise().selfstatus.status) {
        /* This case is true when the task want to push some
         * task to the queue and reschedule the current task
         * we can aslo push task buy passing the eventloop
         * instance to the task and the push from there
         * and must be done via eventloop instance that directly
         * associating with queue;
         */
      case BUFFIO_ROUTINE_STATUS_PAUSED:
        queue->pushroutine(promise->pushhandle);

        /* This case is true when the task want to give control
         * back to the eventloop and we reschedule the task to
         * execute the next task in the queue.
         *
         */

      case BUFFIO_ROUTINE_STATUS_YIELD:
        promise->selfstatus.status = BUFFIO_ROUTINE_STATUS_EXECUTING;
        queue->reschedule(taskinfo);
        break;
        /*  This case is true when the task wants to
         *  wait for certain other operations;
         */
      case BUFFIO_ROUTINE_STATUS_WAITING:
        queue->settaskwaiter(
            taskinfo,
            promise->waitingfor); // pushing task to waiting map
        break;
        /* These cases follow the same handling process -
         *
         *  1) BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
         *  2) BUFFIO_ROUTINE_STATUS_ERROR:
         *  3) BUFFIO_ROUTINE_STATUS_DONE:
         *
         */
      case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
      case BUFFIO_ROUTINE_STATUS_ERROR:
      case BUFFIO_ROUTINE_STATUS_DONE:
        auto *handle =
            queue->poptaskwaiter(taskinfo); // pulling out any task awaiter in
                                            // case of taskdone or taskerror

        if (handle != nullptr) {
          // setting the waiter status to executing;
          handle->handle.promise().selfstatus.status =
              BUFFIO_ROUTINE_STATUS_EXECUTING;
          // escalating the task error to the parent if there any error;
          instance->buffioescalatetaskerror(handle, taskinfo);
        }

        // destroying task handle if task errored out and task done
        taskhandle.destroy();
        /*
         * removing the task from execqueue and putting in freequeuelist
         * to reuse the task allocated chunk later and prevent allocation
         * of newer chunk every time.
         */
        queue->poptask(taskinfo);
        break;
      };
    };
    BUFFIO_INFO(" Queue empty: no task to execute ");
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
  buffioinstance() : ieventlooptype(BUFFIO_EVENTLOOP_DOWN) {}
  ~buffioinstance() {
    BUFFIO_INFO("Total executed task : ", iqueue.taskn());
    // clean up-code;
  };

  int push(buffioroutine routine) {
    switch (ieventlooptype) {
    case BUFFIO_EVENTLOOP_DOWN:
    case BUFFIO_EVENTLOOP_SYNC:
      iqueue.pushroutine(routine);
      break;
    case BUFFIO_EVENTLOOP_ASYNC:
      break;
    }
    return 0;
  };

private:
 void buffioescalatetaskerror(buffiotaskinfo *to, buffiotaskinfo *from) {
    if (to == nullptr || from == nullptr)
      return;
     to->handle.promise().childstatus = from->handle.promise().selfstatus;
  };
  buffioqueue<buffiotaskinfo, buffioroutine> iqueue;
  enum BUFFIO_EVENTLOOP_TYPE ieventlooptype;
};

#endif
#endif
