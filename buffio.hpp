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

struct buffioinfo{
 const char *address;
 int portnumber;
 int listenbacklog;
 size_t capacity;
 size_t reserve;
 BUFFIO_SOCK_TYPE socktype;
 BUFFIO_FAMILY_TYPE sockfamily;
};


struct buffiopromise;
struct buffioqueue;
struct buffiobuffer;
struct clientinfo;
struct acceptreturn;
using buffiopromise = struct buffiopromise;
using buffioinfo = struct buffioinfo;

#define iowait co_await
#define ioyeild co_yield
#define ioreturn co_return

namespace buffio{
 class buffsocket;
 class sockbroker;
 class instance;
 class queue;
};

#if defined(BUFFIO_IMPLEMENTATION)

// for public use;
struct buffiopromise;
using buffiohandleroutine = std::coroutine_handle<buffiopromise>;

struct buffioqueue{
 buffiohandleroutine handle;
 buffiohandleroutine complitioncallback;
 int priority;
 int priorityexpire; // used to control how much subsiquent call has to be made to this thing
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
 
  buffsocket(buffioinfo &ioinfo) : linfo(ioinfo), clienthandler(nullptr){
     if(ioinfo.capacity < 1){
       BUFFIO_ERROR(" SOCKET: Field maxclient in buffioinfo structure cannot be less than 1");
     return;
     };
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
       BUFFIO_ERROR(" UNKNOWN TYPE SOCKET CREATION FAILED");
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
     unlink(linfo.address);
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
           portnumber = ntohs(address_in.sin_port);
           aacceptedaddress  = inet_ntoa((struct in_addr)address_in.sin_addr); 
         }
        break;
        case BUFFIO_FAMILY_IPV6:  break;
     }; 


      if(afd  < 0){
         BUFFIO_ERROR(" FAILED TO ACCEPT A CONNECTION \n reason : ",strerror(errno));
         return {.errorcode = -1 , .handle = NULL};
      }
      
      if(!clienthandler) return {.errorcode = 1,.handle = NULL};

      cinfo = {.address = aacceptedaddress , .clientfd = afd,.portnumber = portnumber};

      return {.errorcode = 0 , .handle = clienthandler(cinfo)};
};
 
 int socketfd = -1;
 buffioinfo linfo;
 buffioroutine (*clienthandler)(clientinfo cinfo);

 private:
      clientinfo cinfo;
      socklen_t sockinfolen = 0;
      char *aacceptedaddress = nullptr;
      int afd = -1;
      int portnumber = 0;
};


class buffio::queue{
  public:
    queue(size_t queuesize,size_t reservesize) :  capacity(queuesize), 
             occupiedcapacity(0), currentidx(0),
             execqueue(nullptr), emptyplaces(nullptr),
             reservesize(reservesize)
   { 
       if(queuesize < 1){
         BUFFIO_ERROR(" QUEUE: field queuesize to queue is less than 1," 
                          " it must have to be 1 or greater than 1");
        return;
       }
       if(reservesize < 10){
         BUFFIO_ERROR(" QUEUE: field reservesize to queue is less than 10,"
                          " using no reserve policy");
        reservesize = 0;
       } 
       execqueue = new buffioqueue[queuesize + reservesize];
       emptyplaces = new buffioqueue*[queuesize + reservesize];
             
       if(!execqueue || ! emptyplaces){
           BUFFIO_ERROR(" QUEUE CREATION FAILED ");
           BUFFIO_DEBUG(" Allocation of one of the queue failed ");
           BUFFIO_TRACE(" EXEC QUEUE : ",execqueue," QUEUE emptytracker",emptyplaces);
        return; 
       }

       reservequeue = reservesize < 10 ? nullptr : (execqueue + (queuesize - 1));
       current = execqueue;
       Next = execqueue + 1;
   };

 ~queue(){
    if(execqueue) delete execqueue;
    if(emptyplaces) delete emptyplaces;
  };

 int push(buffioroutine routine){
    if(!(occupiedcapacity < capacity)){

           BUFFIO_WARN(" QUEUE: cannot push instance to queue is full. execution will continue"
                           " when queue is free");
     }
     routine.resume(); 
    return 0;
 };

  void yield(){
     currentidx += 1;
      size_t nidx = (currentidx < capacity) * currentidx;
      if(!execqueue[currentidx].handle){
         BUFFIO_TRACE(" NO ROUTINE AVALILABLE TO EXECUTE ");
         return;
      }
    execqueue[currentidx].handle.resume();
    return;
  };
   
 private:

 buffioqueue *execqueue;
 buffioqueue *reservequeue;
 buffioqueue **emptyplaces;
 buffioqueue  *current;
 buffioqueue  *Next;

 size_t emptyptridx;
 size_t capacity;
 size_t occupiedcapacity;
 size_t reservesize;
 size_t currentidx;
};

class buffio::instance{

public:
  

  void fireeventloop(buffsocket &sock){
     buffio::queue queue(sock.linfo.capacity , sock.linfo.reserve);
     sock.listensock();
     acceptreturn accepted = sock.acceptsock();
     queue.push(accepted.handle);
     queue.yield(); 
    return;
  };
  ~instance(){
   //clean up-code;
  };
};

#endif
#endif
