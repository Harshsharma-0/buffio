#ifndef BUFFIO_WORKER_IOURING
#define BUFFIO_WORKER_IOURING

#include "buffio/defs.hpp"
#include "buffio/config.hpp"
#include "buffio/queue.hpp"
#include <liburing.h>
#include <linux/io_uring.h>
#include <atomic>

struct WorkerState {
  struct io_uring ring;
  buffio::Queue<buffio::op_vec> submit_queue;
  unsigned int ring_size;
  int fd;
};

#endif
