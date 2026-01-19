#ifndef BUFFIO_ENUM
#define BUFFIO_ENUM
#include <cstdint>

enum class buffio_routine_status: uint32_t{
 waiting = 21,
 executing = 22,
 yield = 23,
 error = 24,
 paused = 25,
 push_task = 26,
 done = 27,
 unhandled_exception = 28,
 waiting_io = 29,
};

//[reserved enum 31 - 49]

enum class buffio_eventloop_type:uint32_t{
 synced = 50,
 async  = 51,
};

//[reservec enum 60 - 70]

enum class buffio_sockbroker_state: uint32_t{
 inactive          = 71,
 error             = 72,
 epoll             = 73,
 io_uring          = 74,
 epoll_running     = 75,
 io_uring_running  = 76
};



enum class buffio_thread_status: uint32_t{
 inactive  = 81,
 running   = 82,
 killed    = 83,
 done      = 84,
 error     = 85,
 error_map = 86,
};

enum class buffio_sb_poller_type: uint32_t{
  nono = 91,
  monolithic = 92,
  modular = 93,
  io_uring = 94
};

enum class buffio_sb_worker_policy: uint32_t{
 consume = 101,
 wait = 102,
 hybrid = 103
};

enum class buffio_fd_opcode: uint32_t {
   read = 150,
   write = 151,
   start_poll = 152,
   end_poll = 153,
   page_read = 154,
   page_write = 155
};

// #define sb sockbroker
enum class sb_cfg_flag:uint32_t{
 none = 0,
 workernum_ok    = 1u << 0,
 expectedfds_ok  = 1u << 1,
 queuesize_ok    = 1u << 2,
 pollertype_ok   = 1u << 3,
 workerpolicy_ok = 1u << 4
};

/*
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
*/
#define operator_for sb_cfg_flag
#include "buffiooperator.hpp"

constexpr sb_cfg_flag sb_cfg_ok =
    sb_cfg_flag::workernum_ok |
    sb_cfg_flag::expectedfds_ok |
    sb_cfg_flag::queuesize_ok |
    sb_cfg_flag::pollertype_ok |
    sb_cfg_flag::workerpolicy_ok; 


#endif
