#ifndef BUFF_IO
#define BUFF_IO

//TODO : WRITE QUEUE AND DEFINE SYSTEM TO PROPAGATE THE TASKS TO EXECUTION QUEUE;

#include "./buffiolog.hpp"

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
 BUFFIO_SOCK_ASYNC = SOCK_NONBLOCK, 
};

//user can give number between MAX and MINIMUM;
enum BUFFIO_PRIORITY_VALUE{
 BUFFIO_QUEUE_PRIORITY_MAX = 10024,
 BUFFIO_QUEUE_PRIORITY_MIN = -100,
 BUFFIO_QUEUE_PRIORITY_MEDIUM = 500,
 BUFFIO_QUEUE_PRIORITY_NORMAL = 1000,
};

enum BUFFIO_ROUTINE_STATUS{
  BUFFIO_ROUTINE_WAITING = 1,
  BUFFIO_ROUTINE_EXECUTING,
  BUFFIO_ROUTINE_YIELD,
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

struct buffiopromise;
using buffiopromise = struct buffiopromise;
using buffioinfo = struct buffioinfo;
using buffioqueuepolicy = struct buffioqueuepolicy;
using buffiohandleroutine = std::coroutine_handle<buffiopromise>;

#define iowait co_await
#define ioyeild co_yield
#define ioreturn co_return


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


struct buffioroutine: buffiohandleroutine{
    using promise_type = ::buffiopromise;
};

struct acceptreturn{
 int errorcode;
 buffioroutine handle;
};


struct buffioawaiter{
   bool await_ready() const noexcept{ return false;}
   static void await_suspend(std::coroutine_handle<>) noexcept{};
   static void await_resume() noexcept {};
};

struct buffiopromise{
    int success = 0;
    buffiohandleroutine waitfor;
    buffioroutine self; 

    buffioroutine get_return_object(){
            self = {buffioroutine::from_promise(*this)};
            return self; 
            
    };

    std::suspend_always initial_suspend() noexcept{ return{};}; 
    std::suspend_always final_suspend() noexcept{ return{};};
    buffioawaiter await_transform(buffioroutine waitfor){
     waitfor = waitfor; 
     return {};
    };

    void unhandled_exception() {};
    void return_value(int state){ success = state;};

};


#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include <cstring>


class buffio::buffsocket{

 public:
 
  buffsocket(buffioinfo &ioinfo) : linfo(ioinfo), clienthandler(nullptr),
                                   socketfd(-1) , sockfdblocking(false){
     switch(ioinfo.sockfamily){
      case BUFFIO_FAMILY_LOCAL: if(createlocalsocket(ioinfo,&socketfd) < 0) return;
      break;
      case BUFFIO_FAMILY_IPV4:  if(createipv4socket(ioinfo,&socketfd) < 0) return;
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
      default: BUFFIO_ERROR(" UNKNOWN TYPE SOCKET CREATION FAILED "); return;
      break;
    };
    
    if(!(ioinfo.socktype & BUFFIO_SOCK_ASYNC)) sockfdblocking = true;
             
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
                  BUFFIO_ERROR(" SOCKET BINDING FAILED : ", strerror(errno));
                  BUFFIO_LOG(" PORT : ", ioinfo.portnumber," IP ADDRESS : ", ioinfo.address);
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
                  BUFFIO_ERROR("SOCKET BINDING FAILED : ",strerror(errno));
                  close(*socketfd);
                  *socketfd = -1;
          return -1;
         }
         aacceptedaddress = (char *)ioinfo.address; 
    return 0;
   };

 ~buffsocket(){ 
    if(socketfd > 0){
      if(close(socketfd) < 0){
        BUFFIO_ERROR(" SOCKET CLOSE FAILURE");
     };
     if(linfo.sockfamily  == BUFFIO_FAMILY_LOCAL) unlink(linfo.address);

     BUFFIO_LOG(" SOCKET CLOSED SUCCESFULLY");
    }
 };

 int listensock(){
    linfo.listenbacklog = linfo.listenbacklog < 0 ? 0 : linfo.listenbacklog;
    if(listen(socketfd,linfo.listenbacklog) < 0){
        BUFFIO_ERROR(" FAILED TO LISTEN ON SOCKET \n reason : ",strerror(errno));
        return -1;
    }
        BUFFIO_INFO(" listening on socket : \n" 
                        "              address - ",linfo.address,
                        "\n              port - ",linfo.portnumber);

    return 0;
  };

acceptreturn acceptsock(){

      switch(linfo.sockfamily){
        case BUFFIO_FAMILY_LOCAL:  afd = accept(socketfd,NULL,NULL); break;
        case BUFFIO_FAMILY_IPV4: { 
           sockinfolen = sizeof(struct sockaddr_in); 
           struct sockaddr_in address_in;
           afd = accept(socketfd,(struct sockaddr*)&address_in,&sockinfolen);
           if(afd > 0){
             portnumber = ntohs(address_in.sin_port);
             aacceptedaddress  = inet_ntoa((struct in_addr)address_in.sin_addr); 
           }
         }
        break;
        case BUFFIO_FAMILY_IPV6:  break;
     }; 

    if(afd < 0){
       if(errno == EAGAIN || errno == EWOULDBLOCK)
             return {.errorcode = BUFFIO_ACCEPT_STATUS_NA , .handle = NULL};
      
       BUFFIO_ERROR(" Failed to accept connection, \n reason: ",strerror(errno),errno);
             return {.errorcode = BUFFIO_ACCEPT_STATUS_ERROR , .handle = NULL};
    }
  
      cinfo = {.address = aacceptedaddress , .clientfd = afd,.portnumber = portnumber};
      if(!clienthandler) return {.errorcode = BUFFFIO_ACCEPT_STATUS_NO_HANDLER,.handle = NULL};
      return {.errorcode = BUFFIO_ACCEPT_STATUS_SUCCESS , .handle = clienthandler(cinfo)};
};
 
 int socketfd;
 bool sockfdblocking;

 buffioinfo linfo;
 buffioroutine (*clienthandler)(clientinfo cinfo);
 clientinfo cinfo;

 private:
      socklen_t sockinfolen = 0;
      char *aacceptedaddress = nullptr;
      int afd = -1;
      int portnumber = 0;
};


struct buffiotaskchunk{
       int priority{0};
       int busychunk{0};
       buffioroutine handle;
       buffioroutine complitioncallback;
       buffiotaskchunk *next{nullptr};
       buffiotaskchunk *prev{nullptr};
};

class buffioqueuechunk{

public:
  struct buffiotaskchunk *allocatedchunk;
  struct buffiotaskchunk *freetaskchunk;
  struct buffiotaskchunk *execqueue;
  struct buffiotaskchunk *execqueuetail;
  size_t currentat;
  size_t chunkcapacity;
  size_t chunkfilled;
  struct buffioqueuechunk *next;
  struct buffioqueuechunk *prev;

  bool isfull(){ return chunkfilled < chunkcapacity ? false : true;}

  buffioqueuechunk(size_t capacity) : 
      chunkcapacity(capacity), chunkfilled(0),
      allocatedchunk(nullptr), freetaskchunk(nullptr),
      execqueue(nullptr), execqueuetail(nullptr),
      currentat(0),next(nullptr),prev(nullptr)
  {
     allocatedchunk = new buffiotaskchunk[capacity];
     freetaskchunk = allocatedchunk;
     struct buffiotaskchunk *head = allocatedchunk;
       
    for(int i = 1; i < capacity; i++){
     head->next = allocatedchunk + i;
     allocatedchunk[i].prev = head;
     head = head->next;
    }
    BUFFIO_TRACE(" Allocated task chunk for queue chunk, ptraddress : ",allocatedchunk);
    BUFFIO_LOG(" Task queue chunk is ready.");
    BUFFIO_INFO(" Task queue chunk OK. ");
  };

  void clean(){ delete this->allocatedchunk; }
  void pushtask(buffioroutine routine){
   BUFFIO_TRACE(" Pushing routine to task queue");
   freetaskchunk->handle = routine;
   execqueuetail->next = freetaskchunk;
   execqueuetail = freetaskchunk;
   if(freetaskchunk->next)
       freetaskchunk = freetaskchunk->next;

    chunkfilled = (chunkfilled < chunkcapacity) ? chunkfilled  + 1 : chunkcapacity;
   BUFFIO_TRACE(" Pushed routine to task queue");

  };
  void execute(){
   if(execqueue->handle)
     return;
  };
};

//using buffiotaskchunk = struct buffiotaskchunk;
using buffioqueuechunk = struct buffioqueuechunk;

class buffio::queue{
  public:
   
    queue(buffioqueuepolicy &queuepolicy) :  capacity(0), 
             occupiedcapacity(0), chunksnumber(0),
             chunkhead(nullptr),chunknext(nullptr),
             queuepolicy(queuepolicy),queueerror(0),
             chunkcurrent(nullptr)
   { 
       if(queuepolicy.queuecapacity < 1){
         BUFFIO_ERROR(" QUEUE: field queuecapacity to queue is less than 1," 
                      " it must have to be 1 or greater than 1");
         queueerror = -1;
         return;
       }

       chunkhead = new buffioqueuechunk(queuepolicy.queuecapacity);
//       chunkhead->initchunk(queuepolicy.queuecapacity);
       chunkempty = chunkhead;
       chunkcurrent = chunkhead;
       chunkhead->next = nullptr;
       chunknext = chunkhead;
       chunksnumber += 1;
       BUFFIO_INFO(" Queue full, cannot push client handler.");

   };

 ~queue(){
   struct buffioqueuechunk *next;
   for(auto *itr = chunkhead ; itr != nullptr ; ){
       itr->clean();
       next = itr->next;
       delete itr;
       itr = next;
    }
        
};

 int push(buffioroutine routine){
    if(!chunkempty->isfull()){
         BUFFIO_INFO(" Pushed routine to queue"); 
         chunkempty->pushtask(routine);

      return 0;
    };

    switch(queuepolicy.overflowpolicy){
        case OVERFLOWPOLICY_NONE:{
         BUFFIO_INFO(" Queue full, cannot push client handler.");
         BUFFIO_LOG(" Current Queue Policy : OVERFLOWPOLICY_NONE.");
         BUFFIO_TRACE(" Destroying Routine Handle");
         routine.destroy();
        } break;

        case OVERFLOWPOLICY_ALLOCATE_NEW:{ 
         BUFFIO_INFO("All queuechunk busy, Allocating new queue chunk");
         buffioqueuechunk *tmpchunk = new buffioqueuechunk(queuepolicy.queuecapacity);
         chunknext->next = tmpchunk;
//         tmpchunk->initchunk(queuepolicy.queuecapacity);
         chunknext = tmpchunk; 
         BUFFIO_TRACE(" Current Queue Policy : OVERFLOWPOLICY_DISCARD_ROUTINE. chunk : ",tmpchunk);

        }break; 
        case OVERFLOWPOLICY_DISCARD_ROUTINE:{ 
                BUFFIO_INFO(" Cannot push client to queue, queuepolicy restrict that.");
                BUFFIO_LOG(" Current Queue Policy : OVERFLOWPOLICY_DISCARD_ROUTINE.");
                BUFFIO_TRACE(" Destroying Routine Handle");
                routine.destroy();                 
         } break;
        case OVERFLOWPOLICY_EXEC_AFTER_FREE:{

        } break;

    };
    return 0;
 };

  void yield(){
      chunkcurrent->execute();
    return;
  };

  int queueerror;
  
 private:

 buffioqueuechunk *chunkhead ,*chunknext;
 buffioqueuechunk *chunkcurrent, *chunkempty;
 buffioqueuechunk *chunkexecfree, *chunkexecfreetail;

 buffioqueuepolicy queuepolicy;

 size_t chunksnumber;
 size_t capacity;
 size_t occupiedcapacity;
};

class buffio::instance{

public:
  
  static int eventloop(void *data){
      buffio::instance *instance = (buffio::instance *)data;
      buffio::queue queue(instance->instancequeuepolicy);   
      if(queue.queueerror < 0)
         return 0;

     acceptreturn accepted = instance->instancesock->acceptsock();
     switch(accepted.errorcode){
      case  BUFFIO_ACCEPT_STATUS_ERROR: BUFFIO_LOG(" Error while accepting a connection");
      case  BUFFFIO_ACCEPT_STATUS_NO_HANDLER: queue.yield(); break;
      case  BUFFIO_ACCEPT_STATUS_SUCCESS: queue.push(accepted.handle); break;
      case  BUFFIO_ACCEPT_STATUS_NA: queue.yield(); break;
      default: queue.yield();
     };
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
