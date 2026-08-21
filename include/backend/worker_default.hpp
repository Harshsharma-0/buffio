#ifndef BUFFIO_WORKER_DEFAULT
#define BUFFIO_WORKER_DEFAULT

#include "buffio/defs.hpp"
#include "buffio/lfqueue.hpp"
#include "buffio/thread.hpp"
#include "buffio/file.hpp"
#include "buffio/config.hpp"

#include <cstring>
#include <atomic>
#include <latch>


using WorkQueue= 
     buffio::lfQueue<buffio::op_vec>;

using SleepQueue = buffio::lfQueue<std::varient<buffio::semaphore,int>,
                        buffio::lfMemMode::stack,BUFFIO_SLEEP_QUEUE_ORDER>;

struct WorkerState {
    int worker_count;
    std::atomic<int> sleep_count;
    std::atomic<int> active_count;
    std::atomic<int> control;
    void* workers;
    SleepQueue sleep_queue;
    WorkQueue submit_queue;
    WorkQueue complete_queue;
};

#endif
