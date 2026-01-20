#ifndef BUFFIO_SOCK_BROKER
#define BUFFIO_SOCK_BROKER
/*
 * Error codes range reserved for buffiosockbroker
 *
 *  [1000 - 2000]
 *  1000 <= errorcode <= 2000
 *
 *
 *
 */

#if !defined(BUFFIO_IMPLEMENTATION)
#include "buffioenum.hpp"
#include "buffiolfqueue.hpp"
#include "buffiopromise.hpp"
#include "buffiosock.hpp"
#endif

#include <sys/epoll.h>

#define broker_cfg_nodiscard_msg \
    "The value returned by this function must not be discarded"



enum class sb_error : int {
    none            = 0,
    workernum       = -1000,
    expectedfds     = -1001,
    queuesize       = -1002,
    pollertype      = -1003,
    workerpolicy    = -1004,
    unknown         = -1005,
    epollinstace    = -1006,  
};


struct buffiosockbrokerconf {
    sb_cfg_flag sb_configured;
    int sb_workernum; // number of the workers;
    int sb_expectedfds; // number of the expected fd to consume across thread;
    int sb_queuesize; // mast be of power of 2, size is calculated via 2 ^ queuesize;
    buffio_sb_poller_type sb_pollertype; // type of I/O architecture to use;
    buffio_sb_worker_policy sb_workerpolicy; // policy of for the workers
};

// opcodes comes from the buffiosock.hpp


enum class sb_ep_state:uint32_t{
 none           = 0,
 empty_ok       = 1u,
 entry_ok       = 1u << 1,
 consume_ok     = 1u << 2,
 works_ok       = 1u << 3,
 events_ok      = 1u << 4,
 eventsize_ok   = 1u << 5,
 fd_ok          = 1u << 6,
 thread_ok      = 1u << 7,
 thread_run_ok  = 1u << 8
};

#define operator_for sb_ep_state
#include "buffiooperator.hpp"

constexpr sb_ep_state sb_ep_state_ok = sb_ep_state::empty_ok |
                              sb_ep_state::entry_ok     |
                              sb_ep_state::consume_ok   |
                              sb_ep_state::works_ok     |
                              sb_ep_state::events_ok    |
                              sb_ep_state::eventsize_ok | 
                              sb_ep_state::fd_ok        |
                              sb_ep_state::thread_ok    |
                              sb_ep_state::thread_run_ok;

enum class sb_ep_error: int{
  none        = 0,
  empty       = -1101,
  entry       = -1102,
  consume     = -1103,
  works       = -1104,
  events      = -1105,
  eventsize   = -1106,
  fd          = -1107,
  thread      = -1108,
  thread_run  = -1109,
  epollinstance = -1110,
  op_add        = -1111,
  op_del        = -1112,
  occupied      = -1113
 };


using buffio_sb_queue = buffiolfqueue<buffiofdreq>;

#define bf_io_io_uringfd_ok (1 << 9)
#define bf_io_uconsumed_ok (1 << 1)
#define bf_io_uenterentry_ok (1 << 2)
#define bf_io_uconsumeentry_ok (1 << 3)
#define bf_io_usq_ok (1 << 4)
#define bf_io_ucq_ok (1 << 5)

// not defined yet
#define bf_io_ok 0


class buffiosockbroker {

   // the main thread worker code inlined with the code to support hybrid arch
   // only call to register fd for read write.

   static inline int buffio_ep_thread_poll(int fd,buffio_sb_queue *ep_entry,
                                            buffio_sb_queue *ep_works,
                                            buffio_sb_queue *ep_consume)
   {

      size_t evnt_size = 1024;
      struct epoll_event evnt[1024];

//    int ep_wait = epoll_wait(fd);
    
    return 0;
   }

   static inline int buffio_ep_thread_worker(void* data)
   {
    return 0;
   }

    static int buffio_epoll_poller_modular(void* data)
    { 
    /*
      buffiopollcaller *sb_ep = (buffiopollcaller*)data;
      buffio_sb_queue *ep_entry = sb_ep->ep_entry;
      buffio_sb_queue *ep_works = sb_ep->ep_works;
      buffio_sb_queue *ep_conusme = sb_ep->ep_consume;
    */

      
    //  int err = buffio_ep_thread_poll(0,nullptr,0);
       
      return 0;
    }

    __attribute__((used)) static int buffio_epoll_worker_modular(void* data) { return 0; }
    __attribute__((used)) static int buffio_epoll_monolithic(void* data) {
        for(int i = 0; i < 1000000 ; i++){
          std::cout <<" hello poller"<<std::endl;
         i += 1;
        }

     return 0; 
    }
    __attribute__((used)) static int buffio_iouring_poller(void* data) { return 0; }

public:
    buffiosockbroker():ep_fd(-1),ep_totalfd(0),io_ufd(-1),ep_threads(nullptr){
        sb_state = buffio_sockbroker_state::inactive;
        config.sb_configured = sb_cfg_flag::none;
        sb_ep_state ep_configured = sb_ep_state::none;
    };

    ~buffiosockbroker(){
        switch (sb_state) {
          case buffio_sockbroker_state::epoll_running:
          case buffio_sockbroker_state::epoll:
            shutpoll();
            break;
        }
        return;
    }

    [[nodiscard(broker_cfg_nodiscard_msg)]] 
                sb_error configure(struct buffiosockbrokerconf cfg)
    {
        if (cfg.sb_workernum > 0 && cfg.sb_expectedfds > 0 && 
                           config.sb_configured == sb_cfg_flag::none) {
            if (cfg.sb_queuesize >= BUFFIO_RING_MIN && 
                            cfg.sb_queuesize <= buffioatomix_max_order) {
                config = cfg;
                config.sb_configured = sb_cfg_ok;
                return sb_error::none;
            }
            return sb_error::queuesize;
        }
        return sb_error::workernum;
    }

  sb_error init()
    {
        if (sb_state != buffio_sockbroker_state::inactive)
               return sb_error::unknown;

        if (config.sb_configured == sb_cfg_ok) {
            switch (config.sb_pollertype) {
            case buffio_sb_poller_type::monolithic:
            case buffio_sb_poller_type::modular:{
                sb_ep_error err = configureepoll(); 
                if(err == sb_ep_error::none) 
                    return sb_error::none;
    
               return sb_error::epollinstace;
             }
            break;   
            case buffio_sb_poller_type::io_uring:
            break;
            }
            return sb_error::pollertype;
        }
      return sb_error::unknown;
    }

    int pushreq() { return 0; }
    int popreq() { return 0; }
    bool running(){ 
     if(sb_state != buffio_sockbroker_state::inactive
             && sb_state != buffio_sockbroker_state::error) return true;

      return false;
    };

    buffiosockbroker(const buffiosockbroker&) = delete;
    buffiosockbroker& operator=(const buffiosockbroker&) = delete;

    sb_ep_error poll_fd(struct buffiofdreq_add *entry){
       if(sb_state != buffio_sockbroker_state::inactive 
                              && ep_configured == sb_ep_state_ok){
          switch(entry->opcode){
            case buffio_fd_opcode::start_poll: break;
            case buffio_fd_opcode::end_poll: break;
          }   
         return sb_ep_error::none;
        };
     return sb_ep_error::epollinstance; 
    };

private:
    sb_ep_error configureepoll()
    {
        // code below here is used for epoll instance,

        if(ep_configured == sb_ep_state_ok) return sb_ep_error::occupied;

        int ep_fd_tmp = createepollinstance(&ep_fd);
        if (ep_fd_tmp < 0)
            return sb_ep_error::fd;

        ep_configured |= sb_ep_state::fd_ok;
        ep_configured |= sb_ep_state::entry_ok;
        ep_configured |= sb_ep_state::works_ok;
        ep_configured |= sb_ep_state::consume_ok;
        ep_configured |= sb_ep_state::events_ok;
        ep_totalfd = 0;

        ep_configured |= sb_ep_state::eventsize_ok;

        int i = 0;
    /*
        ep_threads = new buffiothreadinfo[(config.sb_workernum + 1)];

        if (ep_threads == nullptr){
            shutpoll();
            return sb_ep_error::thread;
        };

      switch (config.sb_pollertype) {
      case buffio_sb_poller_type::modular:
            buffiothreadinfo threadinfo_tmp  = {0};
            threadinfo_tmp.stacksize = buffiothread::SD;
            threadinfo_tmp.dataptr = nullptr;
            threadinfo_tmp.callfunc = buffiosockbroker::buffio_epoll_poller_modular;
            ep_threads[0] = threadinfo_tmp; 
            i = 1; 
        break;
      };
        ep_configured |= sb_ep_state::thread_ok;

        for (; i < config.sb_workernum; i++) {
            struct buffiothreadinfo threadinfo_tmp = { 0 };
            threadinfo_tmp.stacksize = buffiothread::SD;
            threadinfo_tmp.dataptr = nullptr;
            threadinfo_tmp.callfunc = buffio_epoll_monolithic;
            ep_threads[i] = threadinfo_tmp;
        }
/*
        if (threadpool.runthreads(config.sb_workernum, ep_threads) < 0){
            shutpoll();
            return sb_ep_error::thread_run;
        }
    */
 
   
        ep_configured |= sb_ep_state::thread_run_ok | sb_ep_state::empty_ok;
        sb_state = buffio_sockbroker_state::epoll;
        return sb_ep_error::none;
    }

    void shutpoll()
    {
        sb_ep_state mask = ep_configured;
        if (ep_has_flag(mask,sb_ep_state::fd_ok)) {
            close(ep_fd);
            mask &= ~(sb_ep_state::fd_ok); // unsetting the mask;
        }
        if (ep_has_flag(mask,sb_ep_state::entry_ok))
            mask &= ~(sb_ep_state::entry_ok);
        
        if (ep_has_flag(mask,sb_ep_state::works_ok)) 
            mask &= ~(sb_ep_state::works_ok);
        
        if (ep_has_flag(mask,sb_ep_state::consume_ok))
            mask &= ~(sb_ep_state::consume_ok);
    
            mask &= ~(sb_ep_state::events_ok);

        if (ep_has_flag(mask,sb_ep_state::thread_ok)) {
            if (ep_has_flag(mask,sb_ep_state::thread_run_ok)) {
                    mask &= ~(sb_ep_state::thread_run_ok);
            }
            delete[] ep_threads;
            ep_threads = nullptr;
            mask &= ~(sb_ep_state::thread_ok);
        }
        ep_configured = sb_ep_state::none;
        sb_state = buffio_sockbroker_state::inactive; 
    };

    [[nodiscard]] int createepollinstance(int* fdr)
    {
        int fd = epoll_create1(EPOLL_CLOEXEC);
        if (fd >= 0) {
            *fdr = fd;
            return 0;
      }
        return -1;
    }

    buffiothread threadpool;
    struct buffiosockbrokerconf config;
    buffio_sockbroker_state sb_state;

    std::atomic<int> ep_empty;
    buffio_sb_queue ep_entry;    
    buffio_sb_queue ep_works;    
    buffio_sb_queue ep_consume; 
    pthread_t *ep_threads;
    size_t ep_totalfd;
    int ep_fd;
    sb_ep_state ep_configured;
    int io_ufd;

};

#endif
