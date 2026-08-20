#ifndef BUFFIO_WORKER_IOURING
#define BUFFIO_WORKER_IOURING

#include "buffio/defs.hpp"
#include "buffio/config.hpp"
#include "buffio/lfqueue.hpp"
#include <liburing.h>
#include <linux/io_uring.h>
#include <atomic>

struct WorkerState {
  struct io_uring ring;
  buffio::lfQueue<buffio::op_vec> submit_queue;
  std::atomic<ssize_t> submit_count = 0;
};

#endif
