#ifndef __BUFFIO_SOCK_BROKER__
#define __BUFFIO_SOCK_BROKER__
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

// #define sb sockbroker
enum class sb_cfg_flag:uint32_t{
 none = 0,
 workernum_ok    = 1u << 0,
 expectedfds_ok  = 1u << 1,
 queuesize_ok    = 1u << 2,
 pollertype_ok   = 1u << 3,
 workerpolicy_ok = 1u << 4
};

constexpr sb_cfg_flag operator|(sb_cfg_flag a, sb_cfg_flag b){
  return static_cast<sb_cfg_flag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
};

constexpr sb_cfg_flag operator&(sb_cfg_flag a, sb_cfg_flag b)
{
    return static_cast<sb_cfg_flag>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
};

constexpr sb_cfg_flag operator~(sb_cfg_flag a){
  return static_cast<sb_cfg_flag>(~(static_cast<uint32_t>(a)));
};

constexpr sb_cfg_flag sb_cfg_ok =
    sb_cfg_flag::workernum_ok |
    sb_cfg_flag::expectedfds_ok |
    sb_cfg_flag::queuesize_ok |
    sb_cfg_flag::pollertype_ok |
    sb_cfg_flag::workerpolicy_ok; 


enum class sb_error : int {
    none            = 0,
    workernum       = -1000,
    expectedfds     = -1001,
    queuesize       = -1002,
    pollertype      = -1003,
    workerpolicy    = -1004,
    unknown         = -1005,
    epollinstace    = -1006
};


struct buffiosockbrokerconf {
    sb_cfg_flag sb_configured;
    int sb_workernum; // number of the workers;
    int sb_expectedfds; // number of the expected fd to consume across thread;
    int sb_queuesize; // mast be of power of 2, size is calculated via 2 ^ queuesize;
    enum BUFFIO_SOCKBROKER_POLLER_TYPE sb_pollertype; // type of I/O architecture to use;
    enum BUFFIO_SOCKBROKER_WORKER_POLICY sb_workerpolicy; // policy of for the workers
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

constexpr sb_ep_state operator|(sb_ep_state a, sb_ep_state b){
  return static_cast<sb_ep_state>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
};

constexpr sb_ep_state& operator|=(sb_ep_state &lhs, sb_ep_state rhs){
    lhs = lhs | rhs;
    return lhs;
};

constexpr sb_ep_state operator&(sb_ep_state a, sb_ep_state b)
{
    return static_cast<sb_ep_state>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
};

constexpr sb_ep_state& operator&=(sb_ep_state &lhs, sb_ep_state rhs)
{
    lhs = lhs & rhs;
    return lhs;
};

constexpr sb_ep_state operator~(sb_ep_state a){
  return static_cast<sb_ep_state>(~(static_cast<uint32_t>(a)));
};


constexpr static inline bool ep_has_flag(sb_ep_state val,sb_ep_state flag){
  return ((static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0);
};

constexpr sb_ep_state sb_ep_state_ok = sb_ep_state::empty_ok |
                              sb_ep_state::entry_ok     |
                              sb_ep_state::consume_ok   |
                              sb_ep_state::works_ok     |
                              sb_ep_state::events_ok    |
                              sb_ep_state::eventsize_ok | 
                              sb_ep_state::fd_ok        |
                              sb_ep_state::thread_ok   |
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
};


using buffio_sb_queue = buffiolfqueue<buffiofdreq>;

struct buffioepollcaller {
    std::atomic<int>* ep_empty; // reserved for future use;
    buffio_sb_queue* ep_entry; // ep_entry are the entry that you want to add to the epoll instance;
    buffio_sb_queue* ep_works; // ep_works are the entry that are pushed to the worker thread;
    buffio_sb_queue* ep_consume; // ep_consume are the entry that are processed;
    struct epoll_event* ep_events;
    buffiothreadinfo* ep_threads;
    size_t ep_eventsize;
    size_t ep_totalfd;
    int ep_fd;
    sb_ep_state ep_configured;
};

#define bf_io_io_uringfd_ok (1 << 9)
#define bf_io_uconsumed_ok (1 << 1)
#define bf_io_uenterentry_ok (1 << 2)
#define bf_io_uconsumeentry_ok (1 << 3)
#define bf_io_usq_ok (1 << 4)
#define bf_io_ucq_ok (1 << 5)

// not defined yet
#define bf_io_ok 0

struct buffioiouringcaller {
    int io_uconfigured;
    int io_uringfd;
    std::atomic<int>* io_uconsumed;
    buffiolfqueue<void*>* io_uentry; // used to enter entry for i/o operation
    buffiolfqueue<void*>* io_uconsume; // used to dispatch tasks that are done to the schedular
};

union sockbrokerinfo {
    struct buffioepollcaller epollinfo;
    struct buffioiouringcaller iouringinfo;
};


class buffiosockbroker {

// the main thread worker code inlined with the code to support hybrid arch
   static inline int buffio_ep_thread_poll(void* data)
   {
    return 0;
   }
   static inline int buffio_ep_thread_worker(void* data)
   {
    return 0;
  }

    static int buffio_epoll_poller_modular(void* data)
    {
       for(int i = 0; i < 100000 ; i ++){
        std::cout <<" hello poller"<<std::endl;
      }
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
    buffiosockbroker()
    {
        sb_state = buffio_sockbroker_state::inactive;
        config.sb_configured = sb_cfg_flag::none;
    };
    ~buffiosockbroker()
    {
        switch (sb_state) {
          case buffio_sockbroker_state::epoll_running:
          case buffio_sockbroker_state::epoll:
            shutepoll(brkinfo.epollinfo);
            break;
        }
        return;
    }

    [[nodiscard(broker_cfg_nodiscard_msg)]] int configure(struct buffiosockbrokerconf cfg)
    {
        if (cfg.sb_workernum > 0 && cfg.sb_expectedfds > 0 && 
                           config.sb_configured == sb_cfg_flag::none) {
            if (cfg.sb_queuesize >= BUFFIO_RING_MIN && 
                            cfg.sb_queuesize <= buffioatomix_max_order) {
                config = cfg;
                config.sb_configured = sb_cfg_ok;
                return static_cast<int>(sb_error::none);
            }
            return static_cast<int>(sb_error::queuesize);
        }
        return static_cast<int>(sb_error::workernum);
    }
    // if there is any error the relative mask_ok of the field is returned that caused the error;
    // if success the realtive bf_(which)_ok is returned; which can be ep for epoll or io for io_uring
    int init()
    {
        if (sb_state != buffio_sockbroker_state::inactive)
               return static_cast<int>(sb_error::unknown);

        if (config.sb_configured == sb_cfg_ok) {
            switch (config.sb_pollertype) {
            case BUFFIO_POLLER_MONOLITHIC:
            case BUFFIO_POLLER_MODULAR:{
                sb_ep_error err = configureepoll(); 
                if(err == sb_ep_error::none) 
                    return static_cast<int>(sb_error::none);
    
               return static_cast<int>(sb_error::epollinstace);
             }
            break;   
            case BUFFIO_POLLER_IO_URING:
                break;
            }
            return static_cast<int>(sb_error::pollertype);
        }
      return static_cast<int>(sb_error::unknown);
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

private:
    sb_ep_error configureepoll()
    {
        // code below here is used for epoll instance,

        struct buffioepollcaller epolltmp = { 0 };

        int ep_fd_tmp = createepollinstance(&epolltmp.ep_fd);
        if (ep_fd_tmp < 0)
            return sb_ep_error::fd;

        epolltmp.ep_configured |= sb_ep_state::fd_ok;

        epolltmp.ep_entry = new  buffio_sb_queue;
        if (epolltmp.ep_entry == nullptr) {
            shutepoll(epolltmp);
            return sb_ep_error::entry;
        }

        epolltmp.ep_configured |= sb_ep_state::entry_ok;
        epolltmp.ep_works = new  buffio_sb_queue;

        if (epolltmp.ep_works == nullptr) {
            shutepoll(epolltmp);
            return sb_ep_error::works;
        }

        epolltmp.ep_configured |= sb_ep_state::works_ok;
        epolltmp.ep_consume = new  buffio_sb_queue;
        if (epolltmp.ep_consume == nullptr) {
            shutepoll(epolltmp);
            return sb_ep_error::consume;
        }
        epolltmp.ep_configured |= sb_ep_state::consume_ok;
        epolltmp.ep_events = new struct epoll_event[config.sb_expectedfds];
        if (epolltmp.ep_events == nullptr) {
            shutepoll(epolltmp);
            return sb_ep_error::events;
        }

        epolltmp.ep_configured |= sb_ep_state::events_ok;
        epolltmp.ep_eventsize = config.sb_expectedfds;
        epolltmp.ep_totalfd = 0;
        epolltmp.ep_configured |= sb_ep_state::eventsize_ok;

        int i = 0;
        epolltmp.ep_threads = new buffiothreadinfo[(config.sb_workernum + 1)];

        if (epolltmp.ep_threads == nullptr){
            shutepoll(epolltmp);
            return sb_ep_error::thread;
        };

      switch (config.sb_pollertype) {
        case BUFFIO_POLLER_MODULAR:
            buffiothreadinfo threadinfo_tmp  = {0};
            threadinfo_tmp.stacksize = buffiothread::SD;
            threadinfo_tmp.dataptr = nullptr;
            threadinfo_tmp.callfunc = buffiosockbroker::buffio_epoll_poller_modular;
            epolltmp.ep_threads[0] = threadinfo_tmp; 
            i = 1; 
        break;
        };
        epolltmp.ep_configured |= sb_ep_state::thread_ok;

        for (; i < config.sb_workernum; i++) {
            struct buffiothreadinfo threadinfo_tmp = { 0 };
            threadinfo_tmp.stacksize = buffiothread::SD;
            threadinfo_tmp.dataptr = nullptr;
            threadinfo_tmp.callfunc = buffio_epoll_monolithic;
            epolltmp.ep_threads[i] = threadinfo_tmp;
        }
    //      epolltmp.ep_threads[0].callfunc(nullptr);

        if (threadpool.runthreads(config.sb_workernum, epolltmp.ep_threads) < 0){
            shutepoll(epolltmp);
            return sb_ep_error::thread_run;
        }
 
   
        epolltmp.ep_configured |= sb_ep_state::thread_run_ok | sb_ep_state::empty_ok;
        brkinfo.epollinfo = epolltmp;
        sb_state = buffio_sockbroker_state::epoll;
        return sb_ep_error::none;
    }

    void shutepoll(struct buffioepollcaller& which)
    {
        sb_ep_state mask = which.ep_configured;
        if (ep_has_flag(mask,sb_ep_state::fd_ok)) {
            close(which.ep_fd);
            mask &= ~(sb_ep_state::fd_ok); // unsetting the mask;
        }
        if (ep_has_flag(mask,sb_ep_state::entry_ok)) {
            delete which.ep_entry;
            mask &= ~(sb_ep_state::entry_ok);
        }
        if (ep_has_flag(mask,sb_ep_state::works_ok)) {
            delete which.ep_works;
            mask &= ~(sb_ep_state::works_ok);
        }
        if (ep_has_flag(mask,sb_ep_state::consume_ok)) {
            delete which.ep_consume;
            mask &= ~(sb_ep_state::consume_ok);
        }
        if (ep_has_flag(mask,sb_ep_state::events_ok)){
            delete[] which.ep_events;
            mask &= ~(sb_ep_state::events_ok);
        }
        if (ep_has_flag(mask,sb_ep_state::thread_ok)) {
            if (ep_has_flag(mask,sb_ep_state::thread_run_ok)) {
                threadpool.killthread(-1);
                mask &= ~(sb_ep_state::thread_run_ok);
            }
            delete[] which.ep_threads;
            mask &= ~(sb_ep_state::thread_ok);
        }
        which.ep_configured = mask;
        sb_state = buffio_sockbroker_state::inactive; 
    };

    [[nodiscard]] int createepollinstance(int* fdr)
    {
        int fd = epoll_create(1);
        if (fd > 0) {
            *fdr = fd;
            return 0;
      }
        return -1;
    }

    buffiothread threadpool;
    union sockbrokerinfo brkinfo;
    struct buffiosockbrokerconf config;
    buffio_sb_queue  *sb_input; // give the interested files here
    buffio_sb_queue  *sb_output; // pop out the completed tasks here
    buffio_sockbroker_state sb_state;
};

#endif
