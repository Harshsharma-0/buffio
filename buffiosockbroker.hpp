#ifndef __BUFFIO_SOCK_BROKER__
#define __BUFFIO_SOCK_BROKER__
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

constexpr size_t BUFFIO_EPOLL_MAX_THRESHOLD = 100;

#define buffio_sock_maskfresh(sock) ((sock) > 0)
#define buffio_sock_mark_maskstale(sock) sock = ~0;

#define mask_work(mask,exp) (mask|exp)


struct buffiosockbrockerconf{
 int configured;
 int workernum; // number of the workers;
 int expectedfds; // number of the expected to comsume per thread;
 enum BUFFIO_SOCKBROKER_POLLER_TYPE pollertype; // type of I/O archetecture to use;
 enum BUFFIO_SOCKBROKER_WORKER_POLICY  workerpolicy; // policy of for the workers
};

struct buffiosbrokerinfo {
  std::atomic<int> networksock; // if -1 , sock closed flush any buffer;
  std::atomic<int> event; // if < 0, mask is stale and the entry are consumed;
  void *task;
};

struct buffioepollcaller{
   std::atomic<int> *consumed;
   buffiolfqueue<void*> *enterentry;
   buffiolfqueue<void*> *consumeentry;
   size_t fdcount;
   size_t eventcount;
   size_t epollmaxevent;
   struct epoll_event *events[2];
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

//the main thread worker code inlined with the code to support hybrid archetecture
static inline int buffiothreadworker(void *data){ return 0;}

class buffiosockbroker {
 
  #define pollertypecfd 1
  #define workernumcfd 1 << 1
  #define expectedfdcfd 1 << 2
  #define workerpolicycfd 1 << 3
  #define maskbrokerok (pollertyecfg | workernumcfd | expectedfdcfd | workerpolicycfd)


public:
  buffiosockbroker():sbrokerstate(BUFFIO_SOCKBROKER_INACTIVE){ config.configured = 0;};
  buffiosockbroker(size_t maxevents):sbrokerstate(BUFFIO_SOCKBROKER_INACTIVE){config.configured = 0;};
  
  void operator[](enum BUFFIO_SOCKBROKER_POLLER_TYPE typ){
    config.pollertype = typ;
    config.configured |= pollertypecfd; 
  }
  void operator[](enum BUFFIO_SOCKBROKER_WORKER_POLICY ply){
   config.workerpolicy = ply;
   config.configured |= workerpolicycfd;
  }
  void operator[](int expectedfd,int workernum){
    if(workernum > 0 && expectedfd > 0){
      config.workernum = workernum;
      config.expectedfds = expectedfd;
      config.configured |= expectedfdcfd | workernumcfd;
    }
  }
  int operator[](struct buffiosockbrokerconf cfg){
    if(cfg.workernum > 0 && cfg.expectedfds > 0){
       config = cfg;
       config.configured = maskbrokerok; 
    }
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

  /*
  int start() {
    switch(sbrokerstate) {
      
    case BUFFIO_SOCKBROKER_INACTIVE:
      epollstate.epollfd = epoll_create(1);
      if (epollstate.epollfd < 0) {
        BUFFIO_ERROR(" Failed to create a epoll instance of socker : reason -> ",
                     strerror(errno));
        return BUFFIO_SOCKBROKER_ERROR;
      };
    break;
    case BUFFIO_SOCKBROKER_ACTIVE:
    case BUFFIO_SOCKBROKER_BUSY:
      break;
    }
    return BUFFIO_SOCKBROKER_SUCCESS;
  };
/*
  int push(buffiosbrokerinfo *broker) {
    struct epoll_event event;
    event.events = broker->event;
    event.data.fd = broker->networksock;
    event.data.ptr = broker->task;
  
    switch (sbrokerstate) {
    case BUFFIO_SOCKBROKER_ACTIVE:
      int ret = epoll_ctl(epollstate.epollfd, EPOLL_CTL_ADD, broker->networksock, &event);
      if (ret < 0){
        BUFFIO_ERROR(" Failed to add file descriptor in epoll, reason : ",
                     strerror(errno));
        break;
      }
      return BUFFIO_SOCKBROKER_SUCCESS;
      break;
    }
    return BUFFIO_SOCKBROKER_ERROR;
  };
 // consumed = -1, indicate empty;
 // consumed = 0, all ok;
 // consumed = 1, epoll error;
 // consumed = 2, epoll continue;
 // consumed = 3 epoll event available;
 // consumed = 4 parent consuming epoll;
 
/*
  static int epolllistener(void *data){ 
    struct buffioepollcaller *estate = (struct buffioepollcaller*)data;
    int numfds = epoll_wait(estate->epollfd,estate->events[0],estate->eventcount,-1);  
    if(numfds < 0){
      estate->consumed.store(1, std::memory_order_release);
      BUFFIO_ERROR(" epoll wait error, reason : ", strerror(errno));
    };
     estate->consumed.store(1, std::memory_order_release);
    return 0; 
};
/*

  ~buffiosockbroker() {

    switch (sbrokerstate) {
    case BUFFIO_SOCKBROKER_INACTIVE: return;
    case BUFFIO_SOCKBROKER_ACTIVE:  
         thread.killthread(0);
         close(epollstate.epollfd);
      break;
    case BUFFIO_SOCKBROKER_BUSY:
      break;
    }
  };
*/

