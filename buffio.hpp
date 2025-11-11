#ifndef BUFF_IO
#define BUFF_IO

//TODO : WRITE QUEUE AND DEFINE SYSTEM TO PROPAGATE THE TASKS TO EXECUTION QUEUE;

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

enum BUFFIO_QUEUE_OVERFLOW_POLICY{
 OVERFLOWPOLICY_NONE,
 OVERFLOWPOLICY_ALLOCATE_NEW,
 OVERFLOWPOLICY_DISCARD_ROUTINE,
 OVERFLOWPOLICY_EXEC_AFTER_FREE,
};

enum BUFFIO_QUEUE_CHUNKALLOCATION_POLICY{
 CHUNKALLOCATION_POLICY_NONE,
 CHUNKALLOCATION_POLICY_NEW_CHUNK,
 CHUNKALLOCATION_POLICY_SIZE_DOUBLE,
};

enum BUFFIO_QUEUE_THREAD_POLICY{
 THREAD_POLICY_PER_CHUNK,
 THREAD_POLICY_NONE,
};

enum BUFFIO_QUEUE_ROUTINE_ERROR_POLICY{
 ROUTINE_ERROR_POLICY_NONE,
 ROUTINE_ERROR_POLICY_SHUTDOWN,
 ROUTINE_ERROR_POLICY_CONTINUE,
 ROUTINE_ERROR_POLICY_CALLBACK,
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

struct buffioinfo{
 const char *address;
 int portnumber;
 int listenbacklog;
 int socktype;
 BUFFIO_FAMILY_TYPE sockfamily;
};

struct buffioqueuepolicy{
 enum BUFFIO_QUEUE_OVERFLOW_POLICY overflowpolicy;
 enum BUFFIO_QUEUE_THREAD_POLICY threadpolicy;
 enum BUFFIO_QUEUE_CHUNKALLOCATION_POLICY chunkallocationpolicy;
 enum BUFFIO_QUEUE_ROUTINE_ERROR_POLICY routineerrorpolicy;
 int queuecapacity;
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

#define iowait co_await
#define ioyeild co_yield
#define ioreturn co_return


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

struct buffiopromise{
    enum BUFFIO_ROUTINE_STATUS status;
    int returncode = 0;
    buffioroutine waitingfor;
    buffioroutine self;

    std::exception_ptr routineexception;

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
     return {.self = waitfor};
    };

    void unhandled_exception() {
     status = BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION;
     returncode = -1;
     routineexception = std::current_exception();
    };

   int return_value(int state){ 
       returncode = state;
       status = state < 0 ? BUFFIO_ROUTINE_STATUS_ERROR : BUFFIO_ROUTINE_STATUS_DONE;
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
       switch(evalue.promise().status){
          case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
             evalue.promise().status = BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION_DONE;
             std::rethrow_exception(evalue.promise().routineexception);
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
        evalue.promise().status = BUFFIO_ROUTINE_STATUS_DONE;
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


struct buffiotaskchunk{
       int priority{0};
       int busychunk{0};
       buffioroutine handle;
       buffioroutine complitioncallback;
};


using buffioqueuechunk = struct buffiotaskchunk;

class buffio::queue{
  public:   
    queue(buffioqueuepolicy &queuepolicy)
   { 
       if(queuepolicy.queuecapacity < 1){
         BUFFIO_ERROR(" QUEUE: field queuecapacity to queue is less than 1," 
                      " it must have to be 1 or greater than 1");
         queueerror = BUFFIO_QUEUE_STATUS_ERROR;
         return;
       }
      
   };

 ~queue(){
  };

  int push(buffioroutine routine){return 0;};
  void yield(){};

  int queueerror;
  
 private:

 buffioqueuepolicy queuepolicy; 
 
 size_t capacity;
 size_t occupiedcapacity;
};


class buffio::instance{

public:
  
  static int eventloop(void *data){
      buffio::instance *instance = (buffio::instance *)data;
      buffio:buffsocket *buffsock = instance->instancesock;
      buffio::queue queue(instance->instancequeuepolicy);   
      if(queue.queueerror < 0)
         return 0;

    bool shutdown = false;
      while(shutdown != true){
  
       acceptreturn accepted = buffsock->acceptsock();
       switch(accepted.errorcode){
        case  BUFFIO_ACCEPT_STATUS_ERROR: BUFFIO_LOG(" Error while accepting a connection");
        case  BUFFFIO_ACCEPT_STATUS_NO_HANDLER: queue.yield(); break;
        case  BUFFIO_ACCEPT_STATUS_SUCCESS: queue.push(accepted.handle);
        case  BUFFIO_ACCEPT_STATUS_NA:
        default: queue.yield();
      };

      switch(queue.queueerror){
        case BUFFIO_QUEUE_STATUS_YIELD: if(buffsock->sockfdblocking) buffsock->setfdnonblocking(); break;
        case BUFFIO_QUEUE_STATUS_EMPTY: if(!buffsock->sockfdblocking) buffsock->setfdblocking(); break;
        case BUFFIO_QUEUE_STATUS_SHUTDOWN: shutdown = true; break;
        case BUFFIO_QUEUE_STATUS_CONTINUE: break;
      };
     }
  return 0;
  }
  
  void fireeventloop(buffsocket &sock,buffioqueuepolicy &queuepolicy
                     ,enum BUFFIO_EVENTLOOP_TYPE eventlooptype){

     instancequeuepolicy = queuepolicy;
     instancesock = &sock;
     sock.listensock();
     switch(eventlooptype){
     case EVENTLOOP_SYNC: eventloop(this); break; 
     case EVENTLOOP_ASYNC: break;
     }

     return;
  };

  ~instance(){
   //clean up-code;
  };

private:
  buffsocket *instancesock;
  buffioqueuepolicy instancequeuepolicy;
};

#endif
#endif
