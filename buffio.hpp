#ifndef BUFF_IO
#define BUFF_IO

//TODO : WRITE QUEUE AND DEFINE SYSTEM TO PROPAGATE THE TASKS TO EXECUTION QUEUE;

#if defined(BUFFIO_DEBUG)
  #include "./buffiolog.hpp"
#endif

#include <sys/socket.h>
#include <coroutine>


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
};

struct buffioinfo{
 const char *address;
 int portnumber;
 size_t maxclient;
 BUFFIO_SOCK_TYPE socktype;
 BUFFIO_FAMILY_TYPE sockfamily;
};


struct buffiopromise;
using buffiopromise = struct buffiopromise;
using buffioinfo = struct buffioinfo;
using BUFFIO_FAMILY_TYPE = enum BUFFIO_FAMILY_TYPE;
using BUFFIO_SOCK_TYPE = enum BUFFIO_SOCK_TYPE;

namespace buffio{
 class buffsocket;
 class sockbroker;
 class instance;
 class queue;
};

#if defined(BUFFIO_IMPLEMENTATION)

// for public use;
struct promise_type;
using buffiohandleroutine = std::coroutine_handle<promise_type>;

struct buffiopromise{

  // declatation to compiler to use
   struct promise_type;
   using buffiohandleroutine = std::coroutine_handle<promise_type>;

   //handle for the croutine itself;
   buffiohandleroutine handle;
   //handle to track which is waiting for this instance;
   buffiohandleroutine waiter;

   buffiopromise(buffiohandleroutine handle): handle(handle) {}; 

  struct promise_type{
    int success = 0; 

    buffiohandleroutine get_return_object(){
            return buffiohandleroutine(buffiohandleroutine::from_promise(*this));
    };

    std::suspend_always initial_suspend() noexcept{ return{};}; 
    std::suspend_always final_suspend() noexcept{ return{};};
    void unhandled_exception() {};
    void return_value(int state){ success = state;};
};

    bool await_ready() const noexcept { return false; };

    //handle of the routine that is waiting for completion;
    void await_suspend(buffiohandleroutine waitfor){
       handle.resume();
       waiter = waitfor;
    };
    void await_resume() noexcept {};

};


#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include <cstring>


class buffio::buffsocket{

 public:
 
  buffsocket(buffioinfo &ioinfo) : linfo(ioinfo){
     switch(ioinfo.sockfamily){
      case BUFFIO_FAMILY_LOCAL:
        if(createlocalsocket(ioinfo,&socketfd) < 0)
           return;
      break;
      case BUFFIO_FAMILY_IPV4:
        if(createipv4socket(ioinfo,&socketfd) < 0)
            return;
      break;
      case BUFFIO_FAMILY_IPV6:
      break;
      case BUFFIO_FAMILY_CAN:
      break;
      case BUFFIO_FAMILY_NETLINK:
      break;
      case BUFFIO_FAMILY_LLC:
      break;
      case BUFFIO_FAMILY_BLUETOOTH:
      break;
      default:
       BUFFIO_LOG(ERROR,"UNKNOWN TYPE SOCKET CREATION FAILED");
      return;
      break;
    }; 
     return;
  };
  
   int createipv4socket(buffioinfo &ioinfo , int *socketfd){

      *socketfd = socket(ioinfo.sockfamily,ioinfo.socktype,0);
       struct sockaddr_in addr;
       memset(&addr,'\0',sizeof(sockaddr_in));
       addr.sin_family = ioinfo.sockfamily;
       addr.sin_port = htons(ioinfo.portnumber);
       addr.sin_addr.s_addr = inet_addr(ioinfo.address);
       if(bind(*socketfd, (struct sockaddr *)&addr,sizeof(struct sockaddr_in)) < 0){
                  BUFFIO_LOG(ERROR," SOCKET BINDING FAILED : ", strerror(errno));
                  BUFFIO_LOG(LOG," PORT : ", ioinfo.portnumber," IP ADDRESS : ", ioinfo.address);
                  close(*socketfd);
                  *socketfd = -1;
          return -1;
       }

    return 0;
   }

   int createlocalsocket(buffioinfo &ioinfo ,int *socketfd){
        *socketfd = socket(ioinfo.sockfamily,ioinfo.socktype,0);
         struct sockaddr_un addr;
         memset(&addr,'\0',sizeof(sockaddr_un));
         addr.sun_family = BUFFIO_FAMILY_LOCAL;
         strncpy(addr.sun_path,ioinfo.address,sizeof(addr.sun_path) - 1);
         if(bind(*socketfd, (struct sockaddr *)&addr,sizeof(struct sockaddr_un)) < 0){
                  BUFFIO_LOG(ERROR,"SOCKET BINDING FAILED : ",strerror(errno));
                  close(*socketfd);
                  *socketfd = -1;
          return -1;
         }
          
    return 0;
   };

 ~buffsocket(){ 
    if(socketfd > 0){
      if(close(socketfd) < 0){
        BUFFIO_LOG(ERROR,"SOCKET CLOSE FAILURE");
     };
     unlink(linfo.address);
     BUFFIO_LOG(LOG,"SOCKET CLOSED SUCCESFULLY");
    }
 };

 int socketfd = -1;
 buffioinfo linfo;
};


class buffio::queue{
 
 public:

 queue(size_t queuesize) : capacity(queuesize){
   execQueue = new buffiohandleroutine[queuesize];
   current = execQueue;
   Next = execQueue;
 };


 ~queue(){
    
  };
 int push(){
 
 };
 void cycle(){
  current
 }
 void yield();
  
 private:
 buffiohandleroutine *execQueue;
 size_t capacity;
 size_t *current;
 size_t *Next;
};

class buffio::instance{

public:
 buffiopromise hello(){
     BUFFIO_LOG(TRACE," COROUTINE SPEAKING AWAITING");
     co_return  0;
 };

 buffiopromise test(){
    BUFFIO_LOG(TRACE," COROUTINE SPEAKING");
    co_await hello();
    BUFFIO_LOG(TRACE," COROUTINE SPEAKING");
    co_return 0;
  }

  void fireeventloop(){
    auto ctx = test();
    ctx.handle.resume();
 
    return;
  };

private:

};

#endif
#endif
