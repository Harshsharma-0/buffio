#ifndef BUFF_IO
#define BUFF_IO


#include "./buffiolog.hpp"

#include <sys/socket.h>
#include <exception>
#include <coroutine>
#include <cstdint>
#include <unordered_map>
#include <fcntl.h>

constexpr int BUFFIO_FAMILY_LOCAL = AF_UNIX;
constexpr int BUFFIO_FAMILY_IPV4 = AF_INET;
constexpr int BUFFIO_FAMILY_IPV6 = AF_INET6;
constexpr int BUFFIO_FAMILY_CAN  = AF_CAN;
constexpr int BUFFIO_FAMILY_NETLINK = AF_NETLINK;
constexpr int BUFFIO_FAMILY_LLC = AF_LLC;
constexpr int BUFFIO_FAMILY_BLUETOOTH = AF_BLUETOOTH;

constexpr int BUFFIO_SOCK_TCP = SOCK_STREAM;
constexpr int BUFFIO_SOCK_UDP = SOCK_DGRAM;
constexpr int BUFFIO_SOCK_RAW = SOCK_RAW;
constexpr int BUFFIO_SOCK_ASYNC = SOCK_NONBLOCK; 

enum QUEUE_TASK_STATUS{
 TASK_ERASED = 1,
 TASK_REEXECUTE,
 TASK_POPPED,
 TASK_NONE,
};
enum BUFFIO_QUEUE_POLICY{
  BUFFIO_QUEUE_POLICY_THREADED = 11,
  BUFFIO_QUEUE_POLICY_NONE,
};

enum BUFFIO_ROUTINE_STATUS{
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

enum BUFFIO_TASK_STATUS{
 BUFFIO_TASK_SWAPPED = 31,
 BUFFIO_TASK_WAITER_EXCEPTION_WAITING,
 BUFFIO_TASK_WAITER_EXCEPTION_DONE,
 BUFFIO_TASK_WAITER_NONE,
};

enum BUFFIO_QUEUE_STATUS{
  BUFFIO_QUEUE_STATUS_ERROR = -1,
  BUFFIO_QUEUE_STATUS_SUCCESS = 41,
  BUFFIO_QUEUE_STATUS_YIELD ,
  BUFFIO_QUEUE_STATUS_EMPTY,
  BUFFIO_QUEUE_STATUS_SHUTDOWN,
  BUFFIO_QUEUE_STATUS_CONTINUE,
};

enum BUFFIO_EVENTLOOP_TYPE{
  EVENTLOOP_SYNC = 50,
  EVENTLOOP_ASYNC,
};

enum BUFFIO_ACCEPT_STATUS{
  BUFFIO_ACCEPT_STATUS_ERROR = -1,
  BUFFIO_ACCEPT_STATUS_SUCCESS = 61,
  BUFFIO_ACCEPT_STATUS_NA,
  BUFFFIO_ACCEPT_STATUS_NO_HANDLER,
};

#if defined(BUFFIO_IMPLEMENTATION)

namespace buffio{
 class buffsocket;
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
 int sockfamily;
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
using buffiohandleroutine = std::coroutine_handle<buffiopromise>;
#define buffiowait co_await
#define buffioyeild co_yield
#define buffioreturn co_return
#define buffiopush co_await

struct buffioroutine: buffiohandleroutine{
    using promise_type = ::buffiopromise;
};

struct acceptreturn{
 int errorcode;
 buffioroutine handle;
};


struct buffioawaiter{
   bool await_ready() const noexcept{ return false;}
   void await_suspend(std::coroutine_handle<> h) noexcept{};
   buffioroutine await_resume() noexcept { return self;};
   buffioroutine self;
};


struct buffiopushtaskinfo{
  buffioroutine task;
};

struct buffiopromisestatus{
   enum BUFFIO_ROUTINE_STATUS status;
   int returncode = 0;
   std::exception_ptr routineexception;
};

struct buffiopromise{
    buffioroutine waitingfor;
    buffioroutine pushhandle;
    buffioroutine self;
    buffiopromisestatus childstatus;
    buffiopromisestatus selfstatus;
 
    std::exception_ptr routineexception;
   
    buffioroutine get_return_object(){
            self = {buffioroutine::from_promise(*this)};
            selfstatus.status = BUFFIO_ROUTINE_STATUS_EXECUTING;
            return self;          
    };

    std::suspend_always initial_suspend() noexcept{ return{};}; 
    std::suspend_always final_suspend() noexcept{ return{};};
    std::suspend_always yield_value(int value){
     selfstatus.status = BUFFIO_ROUTINE_STATUS_YIELD;
     return {};
    };

    buffioawaiter await_transform(buffioroutine waitfor){   
     waitingfor = waitfor;
     selfstatus.status = BUFFIO_ROUTINE_STATUS_WAITING;
     return {.self = self};
    };

    buffioawaiter await_transform(buffiopushtaskinfo info){
     pushhandle = info.task;
     selfstatus.status = BUFFIO_ROUTINE_STATUS_PAUSED;
     return {.self = info.task};
    };

    void unhandled_exception() {
     selfstatus.status = BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION;
     selfstatus.returncode = -1;
     selfstatus.routineexception = std::current_exception();
     };

   void return_value(int state){ 
       selfstatus.returncode = state;
       selfstatus.status = state < 0 ?  BUFFIO_ROUTINE_STATUS_ERROR : BUFFIO_ROUTINE_STATUS_DONE;
       return;
    };
    bool checkstatus(){
      return selfstatus.status == BUFFIO_ROUTINE_STATUS_ERROR 
               || selfstatus.returncode < 0 ? true : false;
    };


};

class buffiocatch{

public:
   buffiocatch(buffioroutine self) : evalue(self){ 
    status = self.promise().childstatus;
   };
   void exceptionthrower(){
       switch(status.status){
          case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
            std::rethrow_exception(status.routineexception);
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
       catch(const std::exception &e){handler(e,status.returncode);}
     }
   };
private:
  buffioroutine evalue;
  buffiopromisestatus status;
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


class buffio::queue{
  public:   
    queue(): taskqueue(nullptr),taskcount(0),
             taskqueuetail(nullptr), waitingqueue(nullptr),
             waitingqueuetail(nullptr),freehead(nullptr),freetail(nullptr),
             tasknext(nullptr),activetaskcount(0), waitingtaskcount(0)

   { 
            queueerror = BUFFIO_QUEUE_STATUS_EMPTY;
                    
   };
  
 ~queue(){
   };
   

   buffiotaskinfo* pushroutine(buffioroutine routine){
     taskcount += 1;activetaskcount++; 
     buffiotaskinfo* current = popfreequeue();
     current->next = current->prev = nullptr;
     current->handle = routine;
     pushtaskptr(current,&taskqueue,&taskqueuetail);
     return current;
   };

   void pushtask(buffiotaskinfo *task){
     if (task == nullptr) return;
     if (task->next || task->prev) return;
     taskcount++; activetaskcount++;
     pushtaskptr(task,&taskqueue,&taskqueuetail);
   };

   void escalatetaskerror(buffiotaskinfo *to, buffiotaskinfo *from){
    if(to == nullptr || from == nullptr) return;
    to->handle.promise().childstatus = from->handle.promise().selfstatus;
   };

   buffiotaskinfo* getnexttask() {
     if (!taskqueue) return nullptr; 
     if (tasknext == nullptr) tasknext = taskqueue;
     tasknext = taskqueue;
     return tasknext;
   }
  
  void settaskwaiter(buffiotaskinfo *task, buffioroutine routine){
    buffiotaskinfo *taskexec = pushroutine(routine);
    waitingmap[taskexec] = task;
    waitingtaskcount++; activetaskcount--;
    erasetask(task); // erasing task from execution queue
  };

  buffiotaskinfo* poptaskwaiter(buffiotaskinfo *task){
    if (task == nullptr) return nullptr;
    if(waitingtaskcount == 0) return nullptr;

    auto handle = waitingmap.find(task);
    if(handle == waitingmap.end()) return nullptr;
    waitingmap.erase(task);

    pushtask(handle->second);
    buffiopromise *promise = &handle->second->handle.promise();
    promise->selfstatus.status = BUFFIO_ROUTINE_STATUS_EXECUTING;

    activetaskcount++;
    --waitingtaskcount;
    return handle->second; 
  };

  void poptask(buffiotaskinfo *task){
    if(task == nullptr) return;
    taskcount -= 1;
    erasetask(task);
    pushtofreequeue(task);
  };

  void erasetask(buffiotaskinfo *task){
     if(!task) return;
     if(taskqueue == task && taskqueuetail == task){
        taskqueue = nullptr;
        taskqueuetail = nullptr;
        tasknext = nullptr;
        task->next = task->prev = nullptr;
        taskcount = 0;
        return;
       };
    
      task->prev->next = task->next;
      task->next->prev = task->prev;

     if(task == taskqueue) taskqueue = taskqueue->next;
     if(task == taskqueuetail) taskqueuetail = task->prev;

     task->next = task->prev = nullptr;
     activetaskcount--;

     return;

  };

 
  void yield(){ 
        
    buffiotaskinfo *taskinfo = getnexttask();
    if(taskinfo == nullptr){
      BUFFIO_INFO("NO TASK");
      taskcount = 0;
      queueerror = BUFFIO_QUEUE_STATUS_EMPTY; 
      return;
    }
    buffioroutine taskhandle = taskinfo->handle;
    buffiopromise *promise = &taskhandle.promise();

    // executing task only when status is executing to avoid error;
    if(promise->selfstatus.status == BUFFIO_ROUTINE_STATUS_EXECUTING){taskhandle.resume();}

    switch(taskhandle.promise().selfstatus.status){
      case BUFFIO_ROUTINE_STATUS_PAUSED:
          pushroutine(promise->pushhandle); // pushing a routine provided by the task and rescheduling the task
      case BUFFIO_ROUTINE_STATUS_YIELD:
         promise->selfstatus.status = BUFFIO_ROUTINE_STATUS_EXECUTING;
         erasetask(taskinfo); // erasing task from front
         pushtask(taskinfo); //  pushing task to back;
        break;
       case BUFFIO_ROUTINE_STATUS_WAITING:
          settaskwaiter(taskinfo,promise->waitingfor); // pushing task to waiting map
       break;
       case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
       case BUFFIO_ROUTINE_STATUS_ERROR:
       case BUFFIO_ROUTINE_STATUS_DONE:
         auto *handle = poptaskwaiter(taskinfo); // pulling out any task awaiter in case of taskdone or taskerror
         if(handle != nullptr){
          handle->handle.promise().selfstatus.status = BUFFIO_ROUTINE_STATUS_EXECUTING;
          escalatetaskerror(handle, taskinfo);
         }
         poptask(taskinfo); // removing task from the execqueue;
       break;
     };   
  };

  bool empty(){return (taskcount == 0);}
  size_t taskn(){return taskcount;};
  void setqueuepolicy(buffioqueuepolicy reqpolicy){ }

  int queueerror;

 private:

  void pushtaskptr(buffiotaskinfo *task , buffiotaskinfo **head , buffiotaskinfo **tail){
     
      if(*head == nullptr || *tail == nullptr){
        task->next = task->prev = task;
        *head = *tail = task;
        return;
      }

      task->next = *head;
      task->prev = *tail;
      (*tail)->next = task;
      (*head)->prev = task;
      *tail = task;
  };


  void pushtofreequeue(buffiotaskinfo *task){
    if(task == nullptr) return;
    pushtaskptr(task,&freehead,&freetail);
  };

buffiotaskinfo *popfreequeue(){
   buffiotaskinfo *toreturn = nullptr;
   if(freehead == nullptr || freetail == nullptr){
        toreturn = new buffiotaskinfo;
        return toreturn;
    };

    if(freehead == freetail){
      toreturn = freehead;
      freehead = freetail = nullptr;
      return toreturn;
    }
    toreturn = freehead;
    toreturn->next = toreturn->prev = nullptr;
    if(freehead->next) freehead = freehead->next;
    return toreturn;
  };

 buffiotaskinfo *tasknext;
 buffiotaskinfo *taskqueue , *taskqueuetail;
 buffiotaskinfo *freehead , *freetail;
 std::unordered_map<buffiotaskinfo*,buffiotaskinfo*> waitingmap;
 size_t taskcount;
 size_t activetaskcount , waitingtaskcount;
};

/*
class buffio::socketbroker{

};
*/

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
    switch(eventlooptype){
     case EVENTLOOP_SYNC: 
         queue.pushroutine(routine);
     break; 
     case EVENTLOOP_ASYNC: break;
     }

    return 0;
  };

private:
  buffio::queue queue; 
};

#endif
#endif
