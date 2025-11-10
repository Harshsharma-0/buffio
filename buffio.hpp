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

//user can give number between MAX and MINIMUM;
enum BUFFIO_PRIORITY_VALUE{
 BUFFIO_QUEUE_PRIORITY_MAX = 10024,
 BUFFIO_QUEUE_PRIORITY_MIN = -100,
 BUFFIO_QUEUE_PRIORITY_MEDIUM = 500,
 BUFFIO_QUEUE_PRIORITY_NORMAL = 1000,
};

enum BUFFIO_ROUTINE_STATUS{
  BUFFIO_ROUTINE_STATUS_WAITING = 1,
  BUFFIO_ROUTINE_STATUS_EXECUTING,
  BUFFIO_ROUTINE_STATUS_YIELD,
  BUFFIO_ROUTINE_STATUS_ERROR,
  BUFFIO_ROUTINE_STATUS_PAUSED,
  BUFFIO_ROUTINE_STATUS_DONE,
  BUFFIO_ROUTINE_STATUS_EHANDLER,

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
    bool catcheravailable = false;
    uint32_t waiterid = 0;
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

    buffioawaiter await_transform(buffioroutine waitfor){   
     waitingfor = waitfor;
     status = BUFFIO_ROUTINE_STATUS_WAITING;
     return {.self = waitfor};
    };

    void unhandled_exception() {
     status = BUFFIO_ROUTINE_STATUS_ERROR;
     returncode = -1;
     routineexception = std::current_exception();
    };

   int return_value(int state){ 
       returncode = state;
       status = state < 0 ? BUFFIO_ROUTINE_STATUS_ERROR : BUFFIO_ROUTINE_STATUS_DONE;
       return state;
    };
    void setexceptionflag(bool status){catcheravailable = status;};
    bool getexceptionflag(){return catcheravailable;};
    bool checkstatus(){
      return status == BUFFIO_ROUTINE_STATUS_ERROR || returncode < 0 ? true : false;
    };


};

class buffiocatch{

public:
   buffiocatch(buffioroutine self) : evalue(self){ self.promise().setexceptionflag(true);}

  void operator=(void (*handler)(const std::exception &e , int successcode)){  
    if(evalue.promise().checkstatus()){
       try{std::rethrow_exception(evalue.promise().routineexception);}
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

  // TODO: DO ERROR CHECK IN FUNCTIONS:
  void setfdblocking(){ 
    if(!sockfdblocking){
      int flags = fcntl(socketfd,F_GETFL,0);
      flags &= ~O_NONBLOCK;
      fcntl(socketfd,F_SETFL,flags);
    }
  };
  void setfdnonblocking(){
    if(sockfdblocking){
      int flags = fcntl(socketfd,F_GETFL,0);
      flags |= O_NONBLOCK;
      fcntl(socketfd,F_SETFL,flags);

    }
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

  bool isfull(){ return chunkfilled < chunkcapacity ? false : true;}

  buffioqueuechunk(size_t capacity) : 
      chunkcapacity(capacity), chunkfilled(0),
      allocatedchunk(nullptr), freetaskchunk(nullptr),
      execqueue(nullptr), execqueuetail(nullptr),
      next(nullptr),prev(nullptr),freetasktail(nullptr)
  {
     allocatedchunk = new buffiotaskchunk[capacity];
     freetaskchunk = allocatedchunk;
     struct buffiotaskchunk *head = allocatedchunk;
       
    for(int i = 1; i < capacity; i++){
     head->next = allocatedchunk + i;
     allocatedchunk[i].prev = head;
     head = head->next;
    }
    freetasktail = allocatedchunk + (capacity - 1);
    freetasktail->next = nullptr;

    BUFFIO_TRACE(" Allocated task chunk for queue chunk, ptraddress : ",allocatedchunk);
    BUFFIO_LOG(" Task queue chunk is ready.");
    BUFFIO_INFO(" Task queue chunk OK. ");
  };

  void clean(){ delete this->allocatedchunk; }

  buffiotaskchunk* pushtask(buffioroutine routine){

   BUFFIO_TRACE(" Pushing routine to task queue");
   freetaskchunk->handle = routine;
   if(execqueuetail != nullptr){ execqueuetail->next = freetaskchunk;}
   else{ execqueue = freetaskchunk;}

   execqueuetail = freetaskchunk; 
   execqueuetail->next = execqueue;

   if(freetaskchunk->next)
       freetaskchunk = freetaskchunk->next;

    chunkfilled = (chunkfilled < chunkcapacity) ? chunkfilled  + 1 : chunkcapacity;
    BUFFIO_TRACE(" Pushed routine to task queue");
    return execqueuetail;

  };

  void reschedule(buffiotaskchunk *chunk){
    execqueuetail->next = chunk;
    execqueuetail = chunk;
    execqueue->next = chunk;
    return;
  };

  void reschedule(){
    execqueuetail->next = execqueue;
    execqueuetail = execqueue;
    execqueue = execqueue->next;
  };

   void pushtofree(buffiotaskchunk *chunk){
    if(freetasktail)
       freetasktail->next = chunk;
     freetasktail = chunk;

   };


  void execute(){
  
    if(execqueue == nullptr) return;

   if(!execqueue->handle){
       BUFFIO_INFO(" Execution queue empty");
       BUFFIO_TRACE(" No task in execution queue");
     return;
    }

    buffiohandleroutine handle = execqueue->handle;
  
    if(!handle.done()){
        handle.resume(); 
    }
 

   switch(handle.promise().status){
    case BUFFIO_ROUTINE_STATUS_WAITING:{ 
         BUFFIO_INFO(" Task waiting.",execqueue);
         buffiotaskchunk *raddr = pushtask(handle.promise().waitingfor);
             waitingtaskqueue.insert({execqueue,raddr});
         }
      break;
    case BUFFIO_ROUTINE_STATUS_ERROR:{
        //if routine errorout execute the push parent routine to queue;
          if(!handle.promise().getexceptionflag()){ handle.destroy(); pushtofree(execqueue); }
          else{
                BUFFIO_INFO(" Task error.");
             }
      };
    break;
    case BUFFIO_ROUTINE_STATUS_DONE:{
        execqueue->handle.destroy();

    }
    case BUFFIO_ROUTINE_STATUS_YIELD:{
      if(execqueuetail){
           execqueuetail->next = execqueue;
           execqueue = execqueue->next;
        }
    }
  }

    reschedule();
  
};
  
  bool busy(){
    return (chunkfilled > 0);
  };

  struct buffioqueuechunk *next, *prev;
private:
  buffiotaskchunk *allocatedchunk, *freetaskchunk , *freetasktail;
  buffiotaskchunk *execqueue , *execqueuetail;
  
  std::unordered_map<buffiotaskchunk*,buffiotaskchunk*> waitingtaskqueue;
  size_t chunkcapacity , chunkfilled;
  uint32_t fnvidfactor;
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
         queueerror = BUFFIO_QUEUE_STATUS_ERROR;
         return;
       }

       chunkhead = new buffioqueuechunk(queuepolicy.queuecapacity);
//       chunkhead->initchunk(queuepolicy.queuecapacity);
       chunkempty = chunkhead;
       chunkcurrent = chunkhead;
       chunkhead->next = nullptr;
       chunknext = chunkhead;
       chunksnumber += 1;
      
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
      chunkempty->pushtask(routine);
      BUFFIO_INFO(" Pushed routine to queue"); 

      return BUFFIO_QUEUE_STATUS_SUCCESS;
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
      if(chunkcurrent->busy()){ 
       queueerror = BUFFIO_QUEUE_STATUS_YIELD;
       return;
      }
      queueerror = BUFFIO_QUEUE_STATUS_EMPTY; 

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
      buffio:buffsocket *buffsock = instance->instancesock;
      buffio::queue queue(instance->instancequeuepolicy);   
      if(queue.queueerror < 0)
         return 0;

    bool shutdown = false;

     int count = 0;
   while(count <  10){
     BUFFIO_LOG(" Accepting connection!");

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
      count++;
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
