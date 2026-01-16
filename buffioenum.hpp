#ifndef __BUFFIO_ENUM__
#define __BUFFIO_ENUM__
#include <cstdint>

enum BUFFIO_ROUTINE_STATUS {
  BUFFIO_ROUTINE_STATUS_WAITING = 21,
  BUFFIO_ROUTINE_STATUS_EXECUTING,
  BUFFIO_ROUTINE_STATUS_YIELD,
  BUFFIO_ROUTINE_STATUS_ERROR,
  BUFFIO_ROUTINE_STATUS_PAUSED,
  BUFFIO_ROUTINE_STATUS_PUSH_TASK,
  BUFFIO_ROUTINE_STATUS_DONE,
  BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION,
  BUFFIO_ROUTINE_STATUS_WAITING_IO,
};

enum BUFFIO_TASK_STATUS {
  BUFFIO_TASK_SWAPPED = 31,
  BUFFIO_TASK_WAITER_EXCEPTION_WAITING,
  BUFFIO_TASK_WAITER_EXCEPTION_DONE,
  BUFFIO_TASK_WAITER_NONE,
};

/*
enum BUFFIO_QUEUE_STATUS {
  BUFFIO_QUEUE_STATUS_ERROR = 40,
  BUFFIO_QUEUE_STATUS_SUCCESS = 41,
  BUFFIO_QUEUE_STATUS_YIELD,
  BUFFIO_QUEUE_STATUS_EMPTY,
  BUFFIO_QUEUE_STATUS_SHUTDOWN,
  BUFFIO_QUEUE_STATUS_CONTINUE,
};
*/
enum BUFFIO_EVENTLOOP_TYPE {
  BUFFIO_EVENTLOOP_SYNC = 50, // use this to block main thread
  BUFFIO_EVENTLOOP_ASYNC,     // use this to launch a thread;
 // BUFFIO_EVENTLOOP_SEPERATE,  // use this to create a seperate process from main
  BUFFIO_EVENTLOOP_DOWN,      // indicates eventloop is not running

};

enum BUFFIO_ACCEPT_STATUS {
  BUFFIO_ACCEPT_STATUS_ERROR = 60,
  BUFFIO_ACCEPT_STATUS_SUCCESS = 61,
  BUFFIO_ACCEPT_STATUS_NA,
  BUFFFIO_ACCEPT_STATUS_NO_HANDLER,
};


enum class buffio_sockbroker_state: uint32_t{
 inactive          = 71,
 error             = 72,
 epoll             = 73,
 io_uring          = 74,
 epoll_running     = 75,
 io_uring_running  = 76
};
//constexpr int BUFFIO_THREAD_NOT = 81;
//constexpr int BUFFIO_THREAD_RUNNING = 82;



enum class buffio_thread_status: uint32_t{
 inactive  = 81,
 running   = 82,
 killed    = 83,
 done      = 84,
 error     = 85,
 error_map = 86,
};

enum BUFFIO_SOCKBROKER_POLLER_TYPE{ 
  BUFFIO_POLLER_NONE = 91,
 /* The worker thread here is used as the event poller and publish work for other thread in the global queue
  * if a and every thread that dequeue from the queue alternate between the queue and the fd they have
  */
  BUFFIO_POLLER_MONOLITHIC,
  /*
  * This is used to create a seperate thread for epoll and workers on seperate thread
  * workers wait on a signal from the epoll to consome and wait sleep
  *  
  *  -- WORKERS CAN BE CONFIGURED TO BE ACTIVE AND CONSUME BY THE WORKER POLICY
  */

  BUFFIO_POLLER_MODULAR,
  BUFFIO_POLLER_IO_URING
};

enum BUFFIO_SOCKBROKER_WORKER_POLICY{
 BUFFIO_WORKER_POLICY_CONSUME = 101,
 BUFFIO_WORKER_POLICY_WAIT,
 BUFFIO_WORKER_POLICY_HYBRID
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
#endif
