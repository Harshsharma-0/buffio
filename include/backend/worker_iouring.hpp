#ifndef BUFFIO_WORKER_IOURING
#define BUFFIO_WORKER_IOURING

#include "buffio/defs.hpp"
#include "buffio/lfqueue.hpp"
#include "buffio/thread.hpp"
#include "buffio/file.hpp"
#include "buffio/config.hpp"

#include <cstring>
#include <atomic>
#include <latch>

using WorkQueue= 
     buffio::lfQueue<BGLOBOPVEC,buffio::lfMemMode::stack,4>;

using SleepQueue = buffio::lfQueue<buffio::semaphore *>;


#include <liburing.h>
#include <linux/io_uring.h>

struct WorkerState {
  struct io_uring ring;
  std::atomic<unsigned int> sCount = 0;
  WorkQueue submit_queue;
  std::atomic<ssize_t> subCount = 0;
};

#endif
