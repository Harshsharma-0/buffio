#ifndef __BUFFIO_SOCK_BROKER__
#define __BUFFIO_SOCK_BROKER__

#if !defined(BUFFIO_IMPLEMENTATION)
   #include "buffioenum.hpp"
   #include "buffiopromsie.hpp"
   #include "buffiolfqueue.hpp"
#endif

#include <sys/epoll.h>


#define buffio_msg_broker_cfg_ret_val "The value returned by this operator must not be discarded"

// #define sb sockbroker
#define bf_sb_workernum_ok 1
#define bf_sb_expectedfds_ok (1 << 1)
#define bf_sb_queuesize_ok (1 << 2)
#define bf_sb_pollertype_ok (1 << 3)
#define bf_sb_workerpolicy_ok (1 << 4)
#define bf_sb_ok (bf_sb_workernum_ok | bf_sb_expectedfds_ok | \
                  bf_sb_queuesize_ok | bf_sb_pollertype_ok | bf_sb_workerpolicy_ok)

struct buffiosockbrockerconf{
 int sb_configured;
 int sb_workernum; // number of the workers;
 int sb_expectedfds; // number of the expected fd to conusme across thread;
 int sb_queuesize; // mast be of power of 2, size is calculated via 2 ^ queuesize;
 enum BUFFIO_SOCKBROKER_POLLER_TYPE sb_pollertype; // type of I/O archetecture to use;
 enum BUFFIO_SOCKBROKER_WORKER_POLICY  sb_workerpolicy; // policy of for the workers
};

// used to submit fd to the polling;
// data can be used by worker or the poller to mark an event is available;
// internal poller of the buffiosock uses buffiotaskinfo to mark an event available;

struct buffiosocksubmit{
 int opcode;
 int fd; 
 void *data;
};

struct buffioreq{
  int opcode;
  int fd;
  size_t len;
  char *buffer;
};

union buffiosockreq{
  struct buffiosocksubmit pollfd;
  struct buffioreq writereq;
  struct buffioreq readreq;
};

//#define bf buffio
#define bf_ep_empty_ok 1
#define bf_ep_entry_ok (1 << 1)
#define bf_ep_consume_ok ( 1 << 2)
#define bf_ep_works_ok (1 << 3)
#define bf_ep_events_ok (1 << 4)
#define bf_ep_eventsize_ok (1 << 5)
#define bf_ep_fd_ok (1 << 6)
#define bf_ep_done ( 1 << 7)

#define bf_ep_ok (uint8_t)0xFF


struct buffioepollcaller{
   std::atomic<int> *ep_empty;
   buffiolfqueue<void*> *ep_entry; // ep_entry are the entry that you want to add to the epoll instance;
   buffiolfqueue<void*> *ep_works; // ep_works are the entry that are pushed to the worker thread;
   buffiolfqueue<void*> *ep_consume; // ep_consume are the entry that are processed;
   struct epoll_event *ep_events;
   size_t ep_eventsize;
   size_t ep_totalfd;
   int ep_fd; 
   int ep_configured;
};


#define bf_io_io_uringfd_ok 1
#define bf_io_uconsumed_ok (1 << 1)
#define bf_io_uenterentry_ok (1 << 2)
#define bf_io_uconsumeentry_ok (1 << 3)
#define bf_io_usq_ok (1 << 4)
#define bf_io_ucq_ok (1 << 5)

// not defined yet
#define bf_io_ok 0

struct buffioiouringcaller{
   int io_uconfigured;
   int io_uringfd;
   std::atomic<int> *io_uconsumed;
   buffiolfqueue<void*> *io_uentry; // used to enter entry for i/o operation
   buffiolfqueue<void*> *io_uconsume; // used to dispatch tasks that are done to the schedular

};

union sockbrokerinfo{
   struct buffioepollcaller epollinfo;
   struct buffioiouringcaller iouringinfo;
};

//the main thread worker code inlined with the code to support hybrid arch
static inline int buffio_ep_thread_poll(void *data){
  return 0;
}
static inline int buffio_ep_thread_worker(void *data){ 
  return 0;
}

class buffiosockbroker {

  static int buffio_epoll_poller_moduler(void *data){

    return 0;
  } 
 __attribute__((used))  static int buffio_epoll_worker_moduler(void *data){ return 0;}
 __attribute__((used))  static int buffio_epoll_monolithic(void *data){ return 0;}
 __attribute__((used)) static int buffio_iouring_poller(void *data){ return 0;}
 
public:
  buffiosockbroker():sbrokerstate(BUFFIO_SOCKBROKER_INACTIVE){ config.sb_configured = 0;};
  buffiosockbroker(size_t maxevents):sbrokerstate(BUFFIO_SOCKBROKER_INACTIVE){config.sb_configured = 0;};
  
  [[nodiscard(buffio_msg_broker_cfg_ret_val)]] int operator[](struct buffiosockbrockerconf cfg){
    if(cfg.sb_workernum > 0 && cfg.sb_expectedfds > 0 && config.sb_configured == 0){
       if(cfg.sb_queuesize >= BUFFIO_RING_MIN && cfg.sb_queuesize <= buffioatomix_max_order){
         config = cfg;
         config.sb_configured = bf_sb_ok;
         return bf_sb_ok;
       }
       return bf_sb_queuesize_ok;
    }
    return bf_sb_expectedfds_ok;
  }
  // if there is any error the relative mask_ok of the field is returned that caused the error;
  // if success the realtive bf_(which)_ok is returned; which can be ep for epoll or io for io_uring
  int init(){
    if(sbrokerstate != BUFFIO_SOCKBROKER_INACTIVE) return -1;
    if(config.sb_configured == bf_sb_ok){
      switch(config.sb_pollertype){
        case BUFFIO_POLLER_MONOLITHIC: 
          goto epoll_starter;
          break;
        case BUFFIO_POLLER_MODULER:
          goto epoll_starter;
        break;
        case BUFFIO_POLLER_IO_URING: 
          return 0;
        break;
      }

      // code below here is used for epoll instance,
     epoll_starter:
        int mask = 0;
        brkinfo.epollinfo.ep_configured |= createepollinstance(&brkinfo.epollinfo.ep_fd);
        brkinfo.epollinfo.ep_entry = new buffiolfqueue<void*>;
        brkinfo.epollinfo.ep_works = new buffiolfqueue<void*>;
        brkinfo.epollinfo.ep_consume = new buffiolfqueue<void*>;
        brkinfo.epollinfo.ep_events = new struct epoll_event[config.sb_expectedfds];
        brkinfo.epollinfo.ep_eventsize = config.sb_expectedfds;
        brkinfo.epollinfo.ep_totalfd = 0;
        
        if(brkinfo.epollinfo.ep_entry != nullptr){
         if(brkinfo.epollinfo.ep_entry->lfstart(config.sb_queuesize,nullptr) < 0)
                       return (int)bf_ep_entry_ok;           
         
           mask |= bf_ep_entry_ok;
        }

        if(brkinfo.epollinfo.ep_works != nullptr){
         if(brkinfo.epollinfo.ep_works->lfstart(config.sb_queuesize,nullptr) < 0)
                      return (int)bf_ep_works_ok;         

           mask |= bf_ep_works_ok;
         
        }
        if(brkinfo.epollinfo.ep_consume != nullptr){
         if(brkinfo.epollinfo.ep_consume->lfstart(config.sb_queuesize,nullptr) < 0)
                     return (int)bf_ep_consume_ok;

            mask |= bf_ep_consume_ok;
        }

        if(brkinfo.epollinfo.ep_events == nullptr)
              return (int)bf_ep_events_ok;

       mask |= bf_ep_events_ok | bf_ep_eventsize_ok;
       brkinfo.epollinfo.ep_configured |= mask | bf_ep_done;

      return brkinfo.epollinfo.ep_configured;
    }
    return -1;
  };

private:
 [[nodiscard]]int createepollinstance(int *fdr){
    int fd = epoll_create(1); 
    if(fd > 0){ 
      *fdr = fd;
      return bf_ep_fd_ok;
    }
    return 0; 
  }

  buffiothread threadpool;
  union sockbrokerinfo brkinfo;
  struct buffiosockbrockerconf config;
  buffiolfqueue<void*> *sb_input; // give the interested files here
  buffiolfqueue<void*> *sb_output; // pop out the completed tasks here
  int sbrokerstate;
};


#endif

 // consumed = -1, indicate empty;
 // consumed = 0, all ok;
 // consumed = 1, epoll error;
 // consumed = 2, epoll continue;
 // consumed = 3 epoll event available;
 // consumed = 4 parent consuming epoll;
