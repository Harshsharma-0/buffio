#ifndef BUFF_IO
#define BUFF_IO


#include "./buffiolog.hpp"

#include <sys/socket.h>
#include <exception>
#include <coroutine>
#include <cstdint>
#include <unordered_map>
#include <fcntl.h>

enum BUFFIO_FAMILY_TYPE{
 BUFFIO_FAMILY_LOCAL = AF_UNIX,
 BUFFIO_FAMILY_IPV4 = AF_INET,
 BUFFIO_FAMILY_IPV6 = AF_INET6,
 BUFFIO_FAMILY_CAN  = AF_CAN,
 BUFFIO_FAMILY_NETLINK = AF_NETLINK,
 BUFFIO_FAMILY_LLC = AF_LLC,
 BUFFIO_FAMILY_BLUETOOTH = AF_BLUETOOTH,
};

enum BUFFIO_SOCK_TYPE{
 BUFFIO_SOCK_TCP = SOCK_STREAM,
 BUFFIO_SOCK_UDP = SOCK_DGRAM,
 BUFFIO_SOCK_RAW = SOCK_RAW,
 BUFFIO_SOCK_ASYNC = SOCK_NONBLOCK, 
};

enum QUEUE_TASK_STATUS{
 TASK_ERASED = 1,
 TASK_REEXECUTE,
 TASK_POPPED,
 TASK_NONE,
};
enum BUFFIO_QUEUE_POLICY{
  BUFFIO_QUEUE_POLICY_THREADED,
  BUFFIO_QUEUE_POLICY_NONE,

};

enum BUFFIO_ROUTINE_STATUS{
  BUFFIO_ROUTINE_STATUS_WAITING = 1,
  BUFFIO_ROUTINE_STATUS_EXECUTING,
  BUFFIO_ROUTINE_STATUS_YIELD,
  BUFFIO_ROUTINE_STATUS_ERROR,
  BUFFIO_ROUTINE_STATUS_PAUSED,
  BUFFIO_ROUTINE_STATUS_DONE, 
  BUFFIO_ROUTINE_STATUS_EHANDLER,
  BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION,
  BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION_DONE,
};

enum BUFFIO_TASK_STATUS{
 BUFFIO_TASK_SWAPPED = 1,
 BUFFIO_TASK_WAITER_EXCEPTION_WAITING,
 BUFFIO_TASK_WAITER_EXCEPTION_DONE,
 BUFFIO_TASK_WAITER_NONE,
};

enum BUFFIO_QUEUE_STATUS{
  BUFFIO_QUEUE_STATUS_ERROR = -1,
  BUFFIO_QUEUE_STATUS_SUCCESS = 0,
  BUFFIO_QUEUE_STATUS_YIELD = 1,
  BUFFIO_QUEUE_STATUS_EMPTY,
  BUFFIO_QUEUE_STATUS_SHUTDOWN,
  BUFFIO_QUEUE_STATUS_CONTINUE,
};

enum BUFFIO_EVENTLOOP_TYPE{
  EVENTLOOP_SYNC,
  EVENTLOOP_ASYNC,
};

enum BUFFIO_ACCEPT_STATUS{
  BUFFIO_ACCEPT_STATUS_ERROR = -1,
  BUFFIO_ACCEPT_STATUS_SUCCESS,
  BUFFIO_ACCEPT_STATUS_NA,
  BUFFFIO_ACCEPT_STATUS_NO_HANDLER,
};

#if defined(BUFFIO_IMPLEMENTATION)

namespace buffio{
 class buffsocket;
 class sockbroker;
 class instance;
 class queue;
};

struct buffioqueuepolicy{
  size_t queucapacity;
  enum BUFFIO_QUEUE_POLICY queuepolicy;
};

struct buffioinfo{
 const char *address;
 int portnumber;
 int listenbacklog;
 int socktype;
 BUFFIO_FAMILY_TYPE sockfamily;
};


struct buffiobuffer{
 char *data;
 size_t filled;
 size_t size;
};

struct clientinfo{
 const char *address;
 int clientfd;
 int portnumber;
 buffiobuffer readbuffer;
 buffiobuffer writebuffer;
};


struct buffiopromise;
using buffiopromise = struct buffiopromise;
using buffioinfo = struct buffioinfo;
using buffioqueuepolicy = struct buffioqueuepolicy;
using buffiohandleroutine = std::coroutine_handle<buffiopromise>;
using routinestatus = struct routinestatus;
using clientinfo = struct clientinfo;
using routinestatus = struct routinestatus;
using buffioqueuepolicy = struct buffioqueuepolicy;

#define buffiowait co_await
#define buffioyeild co_yield
#define buffioreturn co_return
#define buffiopush co_await

struct buffioroutine: buffiohandleroutine{
    using promise_type = ::buffiopromise;
};
using buffioroutine = struct buffioroutine;

struct acceptreturn{
 int errorcode;
 buffioroutine handle;
};

using buffiowaitreturn = struct buffiowaitreturn;

struct buffioawaiter{
   bool await_ready() const noexcept{ return false;}
   static void await_suspend(std::coroutine_handle<>) noexcept{};
   buffioroutine await_resume() noexcept { return self;};
   buffioroutine self;
};

using buffioawaiter = struct buffioawaiter;

struct buffiopushtaskinfo{
  buffioroutine task;
};

using buffiopushtaskinfo = struct buffiopushtaskinfo;

struct buffiopromise{
    enum BUFFIO_ROUTINE_STATUS status;
    enum BUFFIO_ROUTINE_STATUS childstatus;
    int returncode = 0;
    int childreturncode = 0;
    buffioroutine waitingfor;
    buffioroutine pushhandle;
    buffioroutine self;

    std::exception_ptr routineexception;
    std::exception_ptr childroutineexception;

    buffioroutine get_return_object(){
            self = {buffioroutine::from_promise(*this)};
            status = BUFFIO_ROUTINE_STATUS_EXECUTING;
            return self;          
    };

    std::suspend_always initial_suspend() noexcept{ return{};}; 
    std::suspend_always final_suspend() noexcept{ return{};};
    std::suspend_always yield_value(int value){
     status = BUFFIO_ROUTINE_STATUS_YIELD;
     return {};
    };

    buffioawaiter await_transform(buffioroutine waitfor){   
     waitingfor = waitfor;
     status = BUFFIO_ROUTINE_STATUS_WAITING;
     return {.self = self};
    };

    buffioawaiter await_transform(buffiopushtaskinfo info){
     pushhandle = info.task;
     status = BUFFIO_ROUTINE_STATUS_PAUSED;
     return {.self = info.task};
    };

    void unhandled_exception() {
     status = BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION;
     returncode = -1;
     routineexception = std::current_exception();
     };

   int return_value(int state){ 
       returncode = state;
       status = state < 0 ?  BUFFIO_ROUTINE_STATUS_ERROR : BUFFIO_ROUTINE_STATUS_DONE;
       return state;
    };
    bool checkstatus(){
      return status == BUFFIO_ROUTINE_STATUS_ERROR || returncode < 0 ? true : false;
    };


};

class buffiocatch{

public:
   buffiocatch(buffioroutine self) : evalue(self){ }
   void exceptionthrower(){
       switch(evalue.promise().childstatus){
          case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
            std::rethrow_exception(evalue.promise().childroutineexception);
          break;
          case BUFFIO_ROUTINE_STATUS_ERROR:
            throw std::runtime_error("error in execution of routine return code less than 0");
          break;
          case BUFFIO_ROUTINE_STATUS_DONE: return; break;
        }
   
   }
  buffiocatch& throwerror(){
    exceptionthrower();
    return *this;
  };

  void operator=(void (*handler)(const std::exception &e , int successcode)){  
    if(evalue.promise().checkstatus()){
    try{
        exceptionthrower();
      }
       catch(const std::exception &e){handler(e,evalue.promise().returncode);}
     }
   };
private:
  buffioroutine evalue;
};



#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include <cstring>
#include "./buffiosock.hpp"


struct buffiotaskinfo{
  buffioroutine handle;
  struct buffiotaskinfo *next;
  struct buffiotaskinfo *prev;
};

using buffiotaskinfo = struct buffiotaskinfo;

class buffio::queue{
  public:   
    queue(): taskqueue(nullptr),taskcount(0),
             taskqueuetail(nullptr), waitingqueue(nullptr),
             waitingqueuetail(nullptr),recenttaskstatus(TASK_ERASED)
   { 
            queueerror = BUFFIO_QUEUE_STATUS_EMPTY;
            queuepolicy.queuepolicy = BUFFIO_QUEUE_POLICY_NONE;
         
   };
  
 ~queue(){
   };
   
   void pushtaskptr(buffiotaskinfo* task){
     if(taskqueue == nullptr && taskqueuetail == nullptr){
        task->next = task->prev = task;
        taskqueue = taskqueuetail = task;
        return;
      }

      task->next = taskqueue;
      task->prev = taskqueuetail;
      taskqueuetail->next = task;
      taskqueue->prev = task;
      taskqueuetail = task;    
      return;
    };

   buffiotaskinfo* pushroutine(buffioroutine routine){
     taskcount += 1;
     buffiotaskinfo* current = new buffiotaskinfo;
     current->handle = routine;
     pushtaskptr(current);
     return current;
   };

   void pushtask(buffiotaskinfo *task){
     pushtaskptr(task);
   };

   void esclatetaskerror(buffiotaskinfo *to, buffiotaskinfo *from){
    if(to == nullptr || from == nullptr){
      return;
    }
    to->handle.promise().childstatus = from->handle.promise().status;
    to->handle.promise().childreturncode = from->handle.promise().returncode;
    to->handle.promise().childroutineexception = from->handle.promise().routineexception;
   };

   buffiotaskinfo* getnexttask() {
     if (!taskqueue) return nullptr;
     switch(recenttaskstatus){
      case TASK_REEXECUTE: return taskqueue; break;
      case TASK_ERASED:
      case TASK_POPPED: return taskqueue; break;
      case TASK_NONE:{ taskqueue = taskqueue->next; return taskqueue; }break;
     }
     return nullptr;
  }

  void settaskwaiter(buffiotaskinfo *task, buffioroutine routine){
    buffiotaskinfo *taskexec = pushroutine(routine);
    waitingmap[taskexec] = task;
  };

  buffiotaskinfo* pushtaskwaiter(buffiotaskinfo *task){

    auto handle = waitingmap.find(task);
    if(handle == waitingmap.end()) return nullptr;
    pushtask(handle->second);
    buffiotaskinfo* taskhandle = handle->second;
    taskhandle->handle.promise().status == BUFFIO_ROUTINE_STATUS_EXECUTING;
    return handle->second;
 
  };

  void poptask(buffiotaskinfo *task){
    taskcount -= 1;
    recenttaskstatus = TASK_POPPED;
    erasetask(task);
  };

  void erasetask(buffiotaskinfo *task){
     recenttaskstatus = TASK_POPPED;
   if(taskqueue == task && taskqueuetail == task){
      taskqueue = nullptr;
      taskqueuetail = nullptr;
      taskcount = 0;
      return;
    };
    
     task->prev->next = task->next;
     task->next->prev = task->prev;


    if(task == taskqueue){
       taskqueue = taskqueue->next;
       return;
     }
     if(task == taskqueuetail)
        taskqueuetail = task->prev;     
  };

  void yield(){ 
        
    buffiotaskinfo *taskinfo = getnexttask();
    if(taskinfo == nullptr){ BUFFIO_INFO("NO TASK");
      queueerror = BUFFIO_QUEUE_STATUS_EMPTY; 
      return;
    }
    buffioroutine taskhandle = taskinfo->handle;
    buffiopromise *promise = &taskhandle.promise();

    if(promise->status == BUFFIO_ROUTINE_STATUS_EXECUTING){
              taskhandle.resume();
      }

    switch(taskhandle.promise().status){
       case BUFFIO_ROUTINE_STATUS_WAITING:{ 
          settaskwaiter(taskinfo,promise->waitingfor); 
          erasetask(taskinfo);
        } break;
       case BUFFIO_ROUTINE_STATUS_YIELD:{ 
         promise->status = BUFFIO_ROUTINE_STATUS_EXECUTING;
         recenttaskstatus = TASK_NONE;
       } break;
       case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
       case BUFFIO_ROUTINE_STATUS_ERROR:{
         auto *handle = pushtaskwaiter(taskinfo); 
         esclatetaskerror(handle, taskinfo);   
         handle->handle.promise().status = BUFFIO_ROUTINE_STATUS_EXECUTING;
         poptask(taskinfo); 
       }break;
       case BUFFIO_ROUTINE_STATUS_PAUSED:
        promise->status = BUFFIO_ROUTINE_STATUS_EXECUTING;
        pushroutine(promise->pushhandle);
        recenttaskstatus = TASK_REEXECUTE;
        break;
       case BUFFIO_ROUTINE_STATUS_DONE:
         auto *handle = pushtaskwaiter(taskinfo);
         esclatetaskerror(handle, taskinfo);   
         if(handle != nullptr) handle->handle.promise().status = BUFFIO_ROUTINE_STATUS_EXECUTING;
         poptask(taskinfo);
       break;
     };   
  };

  bool empty(){ 
      return (taskcount == 0); 
  }

  size_t taskn(){return taskcount;};
  void setqueuepolicy(buffioqueuepolicy reqpolicy){ }

  int queueerror;

 private:

 buffiotaskinfo *taskqueue , *taskqueuetail;
 buffiotaskinfo *waitingqueue ,*waitingqueuetail;
 std::unordered_map<buffiotaskinfo*,buffiotaskinfo*> waitingmap;
 int recenttaskstatus;
 buffioqueuepolicy queuepolicy;
 size_t capacity;
 size_t taskcount;
 size_t buffertracker;
 size_t occupiedcapacity;
};


class buffio::instance{

public:
  
  static int eventloop(void *data){
      buffio::instance *instance = (buffio::instance *)data;
          
      bool shutdown = false;
      while(instance->queue.empty() == false){
          instance->queue.yield();
      };
       BUFFIO_INFO(" Queue empty: no task to execute ");
     
  return 0;
  }
/* 
  int instancepushtask(){
    return 0; 
  };
  void setqueuepolicy(buffioqueuepolicy reqpolicy){
    queue.setqueuepolicy(reqpolicy);
  };

  void operator=(buffioqueuepolicy reqpolicy){
     queue.setqueuepolicy(reqpolicy);
  };
*/

  void fireeventloop(enum BUFFIO_EVENTLOOP_TYPE eventlooptype){
     switch(eventlooptype){
     case EVENTLOOP_SYNC: eventloop(this); break; 
     case EVENTLOOP_ASYNC: break;
     }
     return;
  };

  ~instance(){
   //clean up-code;
  };
  
  int push(buffioroutine routine){
    queue.pushroutine(routine);
    return 0;
  };

private:
  buffio::queue queue; 
};

#endif
#endif
