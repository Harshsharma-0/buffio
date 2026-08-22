#ifndef BUFFIO_WORKER_IOURING_HPP
#define BUFFIO_WORKER_IOURING_HPP

#include "buffio/defs.hpp"
#include "buffio/config.hpp"
#include "buffio/queue.hpp"
#include "buffio/optable.hpp"
#include <liburing.h>
#include <linux/io_uring.h>
#include <atomic>

struct WorkerState {
  struct io_uring ring;
  buffio::Queue<buffio::op_vec> submit_queue;
  unsigned int ring_size;
  int fd;
  ssize_t io_count;
};

#endif
