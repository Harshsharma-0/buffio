#pragma once
#include "Queue.hpp"
#include "clock.hpp"
#include "fd.hpp"
#include "fiber.hpp"
#include "promise.hpp"
#include "sockbroker.hpp"
#include <iostream>

/**
 * @file buffioschedular.hpp
 * @author Harsh Sharma
 * @brief Core scheduler for the BuffIO runtime.
 *
 * This file defines the buffioScheduler class, which acts as the
 * central event loop and task scheduler for BuffIO. It coordinates:
 *
 * - Epoll-based I/O polling
 * - Timer-based wakeups
 * - Task queues
 * - Thread pool execution
 *
 * The scheduler follows a monolithic design where all subsystems
 * (poller, queue, timers, fd-pool) are owned and managed internally.
 */

/**
 * @class buffioScheduler
 * @brief Central event-loop and scheduling engine for BuffIO.
 *
 * @details
 * buffioScheduler is a self-contained runtime that:
 * - Owns the epoll instance
 * - Manages file-descriptor lifecycle via fdPool
 * - Dispatches ready I/O events into work queues
 * - Executes tasks in batches for efficiency
 *
 * The scheduler supports both:
 * - Blocking and non-blocking I/O
 * - Edge-triggered epoll (EPOLLET)
 *
 * Typical lifecycle:
 * 1. Create scheduler
 * 2. Schedule tasks / I/O
 * 3. Call run()
 * 4. Scheduler exits when no work remains
 */

namespace buffio {
class scheduler {
public:
  /**
   * @brief Constructs a new buffioScheduler instance.
   *
   * Initializes internal pools and binds the fd pool
   * to the header pool.
   */

  scheduler();
  /**
   * @brief Default destructor.
   *
   * All resources are released during run() shutdown.
   */
  ~scheduler();

  /**
   * @brief Starts the main event loop.
   *
   * This method:
   * - Initializes internal pools
   * - Starts the poller and thread pool
   * - Enters the main epoll-based event loop
   * - Processes I/O events and scheduled tasks
   *
   * If no tasks or timers are scheduled, the scheduler
   * immediately returns.
   *
   * @return -1 if no work is scheduled
   * @return  0 on successful completion
   * @return  error code on initialization or runtime failure
   */
  int run();
  /**
   * @brief Processes epoll events and dispatches work.
   *
   * Each epoll event contains a pointer to a buffioFd
   * instance. Based on the event type (read/write/error),
   * the fd is routed to the appropriate internal queue.
   *
   * @param[in] evnts Array of epoll events
   * @param[in] len   Number of valid events in the array
   *
   * @return 0 on success
   * @return -1 on error
   */
  int processEvents(struct epoll_event evnts[], int len);
  /**@brief It's a helper function, to enqueue task in execution queue to run
   *
   * helper funcation to dispatch the routine after a successfull/errored
   * read/write on the file descriptor.
   *
   * @param [in] errorCode errorCode to pass the respective handle.
   * @param [in] fd        pointer to the instance of buffiofd.
   * @param [in] header    pointer to the header of the request.
   *
   * @return  0 on Success.
   * @return -1 on Error.
   */

  int dispatchHandle(int errorCode, buffio::Fd *fd, buffioHeader *header);
  /**
   * @brief Executes a fixed number of fd events. only called when any fd is
   * used with epoll edge-triggered
   *
   * Used to prevent starvation and improve fairness
   * between I/O and compute tasks.
   *
   * @param[in] batchSize Maximum number of tasks to execute
   */
  int consumeBatch(int cycle = 8);
  /**@breief cleans the execution queue task and free up the memory.
   *
   */
  void cleanQueue();
  /**
   * @brief Determines the next wake-up time for the event loop.
   *
   * Computes the minimum timeout based on:
   * - Active timers
   * - Pending queued tasks
   *
   * @param[out] exit Set to true if scheduler should terminate
   * @return Timeout in milliseconds for epoll_wait
   */
  int getWakeTime(bool *flag);
  /**
   * @brief method to execute the coroutines.
   *
   * yieldQueue method is used to the task if there's any, and it execute
   * tasks in chunks. means if the chunk is 10, it try to execute 10 tasks, it
   * there task less than 10, then the task is executed in a cyclic way, and
   * based on the status of the task and queue, the yieldQueue returns.
   *
   * it returns only when on of the following happen,
   *  1) while executing the queue become empty.
   *  2) The cycle is finished
   *  3) Some internal error occured.
   *
   * @param[in] chunk the number of tasks to process in one shot.
   * @return return value less than 0 must be treated as error, and must be
   * taken care of.
   */

  int yieldQueue(int chunk);
  /**
   * @brief used to push routine to the execution queue before calling the
   * run.
   *
   * @param[in] handle The handle to the routine.
   *
   * @return returns the value returned by the queue push method. and a value
   * smaller than 0 must be treated as error.
   *
   */
  int push(auto handle) {
    queue.push(handle.get());
    return 0;
  };
  void shutWorker(int workerNum, int tries, long wait);

private:
  buffio::Fd syncPipe;
  buffio::sockBroker poller;
  buffio::Clock timerClock;
  buffio::Queue<> queue;
  buffio::Memory<buffioHeader> headerPool;
  buffio::Queue<buffioHeader, void *, buffioQueueNoMem> requestBatch;
  buffio::thread threadPool;
};
}; // namespace buffio
