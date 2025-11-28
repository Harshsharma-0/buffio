#ifndef __BUFFIO_SOCK_BROKER__
#define __BUFFIO_SOCK_BROKER__
#include <sys/epoll.h>

// socket broker used to listen for events in socket;
// internally uses epoll for all work;
constexpr int BUFFIO_POLL_READ = EPOLLIN;
constexpr int BUFFIO_POLL_WRITE = EPOLLOUT;
constexpr int BUFFIO_POLL_ETRIG = EPOLLET;
constexpr size_t BUFFIO_EPOLL_MAX_THRESHOLD = 100;

enum BUFFIO_SOCKBROKER_STATE {
  BUFFIO_SOCKBROKER_ACTIVE = 71,
  BUFFIO_SOCKBROKER_INACTIVE,
  BUFFIO_SOCKBROKER_BUSY,
  BUFFIO_SOCKBROKER_ERROR,
  BUFFIO_SOCKBROKER_SUCCESS,
};

struct buffiosbrokerinfo {
  int fd;
  int event;
  void *task;
};

class buffiosockbroker {
public:
  
  int start() {
    switch (sbrokerstate) {
      
    case BUFFIO_SOCKBROKER_INACTIVE:
      epollstate.epollfd = epoll_create(1);
      if (epollstate.epollfd < 0) {
        BUFFIO_ERROR(" Failed to create a epoll instance of socker : reason -> ",
                     strerror(errno));
        return BUFFIO_SOCKBROKER_ERROR;
      };
         thread.run();
      break;
    case BUFFIO_SOCKBROKER_ACTIVE:
    case BUFFIO_SOCKBROKER_BUSY:
      break;
    }
    return BUFFIO_SOCKBROKER_SUCCESS;
  };

  int push(buffiosbrokerinfo *broker) {
    struct epoll_event event;
    event.events = broker->event;
    event.data.fd = broker->fd;
    event.data.ptr = broker->task;
  
    switch (sbrokerstate) {
    case BUFFIO_SOCKBROKER_ACTIVE:
      int ret = epoll_ctl(epollstate.epollfd, EPOLL_CTL_ADD, broker->fd, &event);
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
 
  struct epollcaller{
   std::atomic<int> consumed;
   int epollfd;
   size_t fdcount;
   size_t eventcount;
   struct epoll_event *events[2];
   struct epoll_event *available;
   struct epoll_event *free;
  };

  static int epolllistener(void *data) { 
    struct epollcaller *estate = (struct epollcaller*)data;
    int numfds = epoll_wait(estate->epollfd,estate->events[0],estate->eventcount,-1);  
    if(numfds < 0){
      estate->consumed.store(1, std::memory_order_release);
      BUFFIO_ERROR(" epoll wait error, reason : ", strerror(errno));
    };

     estate->consumed.store(1, std::memory_order_release);

    return 0; 
  };

  ~buffiosockbroker() {

    switch (sbrokerstate) {
    case BUFFIO_SOCKBROKER_INACTIVE: return;
    case BUFFIO_SOCKBROKER_ACTIVE:  
         thread.killthread();
         close(epollstate.epollfd);
      break;
    case BUFFIO_SOCKBROKER_BUSY:
      break;
    }
  };

  buffiosockbroker(size_t maxevents)
      : sbrokerstate(BUFFIO_SOCKBROKER_INACTIVE){

     epollstate.events[0] = new struct epoll_event[maxevents];
     epollstate.events[1] = new struct epoll_event[maxevents];
     epollstate.available = nullptr;
     epollstate.free = nullptr;
     epollstate.eventcount = maxevents;
     epollstate.fdcount = 0;
     epollstate.epollfd = -1;
     thread[buffiothread::SD]["buffiosocketbroker"](this) = epolllistener;

  };

private:
  buffiothread thread;
  struct epollcaller epollstate;
  size_t epollmaxevent;
  int sbrokerstate;
};


#endif
