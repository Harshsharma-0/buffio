#ifndef __BUFFIO_SOCK_BROKER__
#define __BUFFIO_SOCK_BROKER__

#if !defined(BUFFIO_IMPLEMENTATION)
   #include "buffioenum.hpp"
   #include "buffiopromsie.hpp"
#endif

#include <sys/epoll.h>

// socket broker used to listen for events in socket;
// internally uses epoll for all work;
// EPOLLIN
// EPOLLOUT
// EPOLLET

constexpr int BUFFIO_POLL_READ =  EPOLLIN;
constexpr int BUFFIO_POLL_WRITE = EPOLLOUT;
constexpr int BUFFIO_POLL_RW = EPOLLIN | EPOLLOUT;
constexpr int BUFFIO_POLL_ETRIG = EPOLLET;

struct buffiosockbrockerconf{
 int configured;
 int workernum; // number of the workers;
 int expectedfds; // number of the expected to comsume per thread;
 enum BUFFIO_SOCKBROKER_POLLER_TYPE pollertype; // type of I/O archetecture to use;
 enum BUFFIO_SOCKBROKER_WORKER_POLICY  workerpolicy; // policy of for the workers
};

// broker request are passed to the worker pool to process the data, or user can mount their own worker 
// thread if they want.
struct buffiosbrokerrequest{
  std::atomic<int> networksock; // if -1 , sock closed flush any buffer;
  std::atomic<int> event; // if < 0, mask is stale and the entry are consumed;
  void *task; // ptr to buffiofd state any request is maintained using fdstate handler;
};


struct buffiobrokerregister{
 int fd; // socket fd
 int notimask; // mask for the type of io request and other things
 void *data; // data to push to queue when io is ready
};

struct buffioepollcaller{
   std::atomic<int> *consumed;
   buffiolfqueue<void*> *enterentry;
   buffiolfqueue<void*> *consumeentry;
   size_t fdcount;
   size_t eventcount;
   size_t epollmaxevent;
   struct epoll_event *events;
   struct epoll_event *available;
   struct epoll_event *free;
   int epollfd;
   int configured;
};

struct buffioiouringcaller{
   int configured;
   int io_uringfd;
   std::atomic<int> *consumed;
   buffiolfqueue<void*> *enterentry; // used to enter entry for i/o operation
   buffiolfqueue<void*> *consumeentry; // used to dispatch tasks that are done to the schedular
};

union sockbrokerinfo{
   struct buffioepollcaller epollinfo;
   struct buffioiouringcaller iouringinfo;
};

//the main thread worker code inlined with the code to support hybrid arch
static inline int buffiothreadworker(void *data){ return 0;}

class buffiosockbroker {
 
  #define pollertypecfd 1
  #define workernumcfd 1 << 1
  #define expectedfdcfd 1 << 2
  #define workerpolicycfd 1 << 3
  #define maskbrokerok (pollertypecfd | workernumcfd | expectedfdcfd | workerpolicycfd)


public:
  buffiosockbroker():sbrokerstate(BUFFIO_SOCKBROKER_INACTIVE){ config.configured = 0;};
  buffiosockbroker(size_t maxevents):sbrokerstate(BUFFIO_SOCKBROKER_INACTIVE){config.configured = 0;};
  
  int operator[](struct buffiosockbrockerconf cfg){
    if(cfg.workernum > 0 && cfg.expectedfds > 0){
       config = cfg;
       config.configured = maskbrokerok; 
       return 0;
    }
    return -1;
  }

private:

  static int buffio_epoll_poller(void *data){ return 0;} 
  static int buffio_epoll_worker(void *data){ return 0;}
  static int buffio_iouring_poller(void *data){ return 0;}

  buffiothread threadpool;
  union sockbrokerinfo brkinfo;
  struct buffiosockbrockerconf config;
  int sbrokerstate;
};


#endif

 // consumed = -1, indicate empty;
 // consumed = 0, all ok;
 // consumed = 1, epoll error;
 // consumed = 2, epoll continue;
 // consumed = 3 epoll event available;
 // consumed = 4 parent consuming epoll;
