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

#define buffio_msg_broker_cfg_ret_val "The value returned by this operator must not be discarded"

// #define sb sockbroker
#define bf_sb_workernum_ok 1
#define bf_sb_expectedfds_ok (1 << 1)
#define bf_sb_queuesize_ok (1 << 2)
#define bf_sb_pollertype_ok (1 << 3)
#define bf_sb_workerpolicy_ok (1 << 4)
#define bf_sb_ok (bf_sb_workernum_ok | bf_sb_expectedfds_ok | bf_sb_queuesize_ok | bf_sb_pollertype_ok | bf_sb_workerpolicy_ok)

#define bf_sb_workernum_err 1000
#define bf_sb_expectedfds_err 1001
#define bf_sb_queuesize_err 1002
#define bf_sb_pollertype_err 1003
#define bf_sb_workerpolicy_err 1004

struct buffiosockbrokerconf {
    int sb_configured;
    int sb_workernum; // number of the workers;
    int sb_expectedfds; // number of the expected fd to consume across thread;
    int sb_queuesize; // mast be of power of 2, size is calculated via 2 ^ queuesize;
    enum BUFFIO_SOCKBROKER_POLLER_TYPE sb_pollertype; // type of I/O architecture to use;
    enum BUFFIO_SOCKBROKER_WORKER_POLICY sb_workerpolicy; // policy of for the workers
};

// opcodes comes from the buffiosock.hpp

// #define bf buffio
#define bf_ep_empty_ok 1
#define bf_ep_entry_ok (1 << 1)
#define bf_ep_consume_ok (1 << 2)
#define bf_ep_works_ok (1 << 3)
#define bf_ep_events_ok (1 << 4)
#define bf_ep_eventsize_ok (1 << 5)
#define bf_ep_fd_ok (1 << 6)
#define bf_ep_thread_ok (1 << 7)
#define bf_ep_thread_run_ok (1 << 8)
#define bf_ep_ok (bf_ep_empty_ok| bf_ep_entry_ok | bf_ep_consume_ok | bf_ep_works_ok \
                  | bf_ep_events_ok |bf_ep_eventsize_ok | bf_ep_fd_ok | bf_ep_thread_ok \
                  | bf_ep_thread_run_ok)

#define bf_ep_empty_err 1101
#define bf_ep_entry_err 1102
#define bf_ep_consume_err 1103
#define bf_ep_works_err 1104
#define bf_ep_events_err 1105
#define bf_ep_eventsize_err 1106
#define bf_ep_fd_err 1107
#define bf_ep_thread_err 1108

#define BUFFIO_SB_QUEUE buffiolfqueue<union buffiofdreq>

struct buffioepollcaller {
    std::atomic<int>* ep_empty; // reserved for future use;
    BUFFIO_SB_QUEUE* ep_entry; // ep_entry are the entry that you want to add to the epoll instance;
    BUFFIO_SB_QUEUE* ep_works; // ep_works are the entry that are pushed to the worker thread;
    BUFFIO_SB_QUEUE* ep_consume; // ep_consume are the entry that are processed;
    struct epoll_event* ep_events;
    buffiothreadinfo* ep_threads;
    size_t ep_eventsize;
    size_t ep_totalfd;
    int ep_fd;
    int ep_configured;
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

// the main thread worker code inlined with the code to support hybrid arch
static inline int buffio_ep_thread_poll(void* data)
{
    return 0;
}
static inline int buffio_ep_thread_worker(void* data)
{
    return 0;
}

class buffiosockbroker {

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
        : sbrokerstate(BUFFIO_SOCKBROKER_INACTIVE)
    {
        config.sb_configured = 0;
    };
    buffiosockbroker(size_t maxevents)
        : sbrokerstate(BUFFIO_SOCKBROKER_INACTIVE)
    {
        config.sb_configured = 0;
    };
    ~buffiosockbroker()
    {
        switch (sbrokerstate) {
        case BUFFIO_SOCKBROKER_EPOLL_RUNNING:
        case BUFFIO_SOCKBROKER_EPOLL:
            shutepoll(brkinfo.epollinfo);
            break;
        }
        return;
    }

    [[nodiscard(buffio_msg_broker_cfg_ret_val)]] int configure(struct buffiosockbrokerconf cfg)
    {
        if (cfg.sb_workernum > 0 && cfg.sb_expectedfds > 0 && config.sb_configured == 0) {
            if (cfg.sb_queuesize >= BUFFIO_RING_MIN && cfg.sb_queuesize <= buffioatomix_max_order) {
                config = cfg;
                config.sb_configured = bf_sb_ok;
                return bf_sb_ok;
            }
            return bf_sb_queuesize_err;
        }
        return bf_sb_expectedfds_err;
    }
    // if there is any error the relative mask_ok of the field is returned that caused the error;
    // if success the realtive bf_(which)_ok is returned; which can be ep for epoll or io for io_uring
    int init()
    {
        if (sbrokerstate != BUFFIO_SOCKBROKER_INACTIVE)
            return -1;
        if (config.sb_configured == bf_sb_ok) {
            switch (config.sb_pollertype) {
            case BUFFIO_POLLER_MONOLITHIC:
            case BUFFIO_POLLER_MODULAR:
                return configureepoll();
            case BUFFIO_POLLER_IO_URING:
                break;
            }
            return bf_sb_pollertype_err;
        }
        return -1;
    }

    int pushreq() { return 0; }
    int popreq() { return 0; }
    bool running(){ 
     if(sbrokerstate != BUFFIO_SOCKBROKER_INACTIVE 
             && sbrokerstate != BUFFIO_SOCKBROKER_ERROR) return true;

      return false;
    };

    buffiosockbroker(const buffiosockbroker&) = delete;
    buffiosockbroker& operator=(const buffiosockbroker&) = delete;

private:
    int configureepoll()
    {
        // code below here is used for epoll instance,

        struct buffioepollcaller epolltmp = { 0 };

        int ep_fd_tmp = createepollinstance(&epolltmp.ep_fd);
        if (ep_fd_tmp == bf_ep_fd_err)
            return bf_ep_fd_err;
        epolltmp.ep_configured |= bf_ep_fd_ok;

        epolltmp.ep_entry = new BUFFIO_SB_QUEUE;
        if (epolltmp.ep_entry == nullptr) {
            shutepoll(epolltmp);
            return bf_ep_entry_err;
        }

        epolltmp.ep_configured |= bf_ep_entry_ok;
        epolltmp.ep_works = new BUFFIO_SB_QUEUE;

        if (epolltmp.ep_works == nullptr) {
            shutepoll(epolltmp);
            return bf_ep_works_err;
        }

        epolltmp.ep_configured |= bf_ep_works_ok;
        epolltmp.ep_consume = new BUFFIO_SB_QUEUE;
        if (epolltmp.ep_consume == nullptr) {
            shutepoll(epolltmp);
            return bf_ep_consume_err;
        }
        epolltmp.ep_configured |= bf_ep_consume_ok;
        epolltmp.ep_events = new struct epoll_event[config.sb_expectedfds];
        if (epolltmp.ep_events == nullptr) {
            shutepoll(epolltmp);
            return bf_ep_events_err;
        }

        epolltmp.ep_configured |= bf_ep_events_ok;
        epolltmp.ep_eventsize = config.sb_expectedfds;
        epolltmp.ep_totalfd = 0;
        epolltmp.ep_configured |= bf_ep_eventsize_ok;

        int i = 0;
        epolltmp.ep_threads = new buffiothreadinfo[(config.sb_workernum + 1)];

        if (epolltmp.ep_threads == nullptr){
            shutepoll(epolltmp);
            return bf_ep_thread_err;
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
        epolltmp.ep_configured |= bf_ep_thread_ok;

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
        }
 
   
        epolltmp.ep_configured |= bf_ep_thread_run_ok | bf_ep_empty_ok;
        brkinfo.epollinfo = epolltmp;
        sbrokerstate = BUFFIO_SOCKBROKER_EPOLL;
        return epolltmp.ep_configured;
    }

    void shutepoll(struct buffioepollcaller& which)
    {
        int mask = which.ep_configured;
        if (mask & bf_ep_fd_ok) {
            close(which.ep_fd);
            mask &= ~(bf_ep_fd_ok); // unsetting the mask;
        }
        if (mask & bf_ep_entry_ok) {
            delete which.ep_entry;
            mask &= ~(bf_ep_entry_ok);
        }
        if (mask & bf_ep_works_ok) {
            delete which.ep_works;
            mask &= ~(bf_ep_works_ok);
        }
        if (mask & bf_ep_consume_ok) {
            delete which.ep_consume;
            mask &= ~(bf_ep_consume_ok);
        }
        if (mask & bf_ep_events_ok) {
            delete[] which.ep_events;
            mask &= ~(bf_ep_events_ok);
        }
        if (mask & bf_ep_thread_ok) {
            if (mask & bf_ep_thread_run_ok) {
                threadpool.killthread(-1);
                mask &= ~(bf_ep_thread_run_ok);
            }
            delete[] which.ep_threads;
            mask &= ~(bf_ep_thread_ok);
        }
        which.ep_configured = mask;
        sbrokerstate = BUFFIO_SOCKBROKER_INACTIVE; 
    };

    [[nodiscard]] int createepollinstance(int* fdr)
    {
        int fd = epoll_create(1);
        if (fd > 0) {
            *fdr = fd;
            return bf_ep_fd_ok;
        }
        return bf_ep_fd_err;
    }

    buffiothread threadpool;
    union sockbrokerinfo brkinfo;
    struct buffiosockbrokerconf config;
    BUFFIO_SB_QUEUE *sb_input; // give the interested files here
    BUFFIO_SB_QUEUE *sb_output; // pop out the completed tasks here
    int sbrokerstate;
};

#endif
