#ifndef BUFFIO_SCHEDULAR
#define BUFFIO_SCHEDULAR

#include "buffioQueue.hpp"
#include "buffioclock.hpp"



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

class buffioScheduler {
 public:

   /**
   * @brief Constructs a new buffioScheduler instance.
   *
   * Initializes internal pools and binds the fd pool
   * to the header pool.
   */
   buffioScheduler() : syncPipe(nullptr) { fdPool.mountPool(&headerPool); };
  /**
   * @brief Default destructor.
   *
   * All resources are released during run() shutdown.
   */
  ~buffioScheduler() = default;

 

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
  int run() {
    if (queue.empty() && timerClock.empty())
      return -1;

    int error = 0;
    if ((error = headerPool.init()) != 0)
      return error;
    if ((error = poller.start(threadPool)) != 0)
      return error;
    if ((syncPipe = fdPool.get()) == nullptr)
      return -1;
    if ((error = buffioMakeFd::pipe(syncPipe)) != 0)
      return error;
    if ((error = poller.pollOp(syncPipe->getPipeRead(), syncPipe)) != 0) {
      syncPipe->release();
      return error;
    };

    struct epoll_event evnt[1024];

    /*
     * batch of fd allocated to the main event loop to process, is just a
     * circular list, as we support EPOLLET edge-triggered mode, then we need
     * batch procssing.
     */
    bool exit = false;
    int timeout = 0;

    while (exit != true) {
      timeout = getWakeTime(&exit);
      if (exit == true)
        break;

      int nfd = poller.poll(evnt, 1024, timeout);
      if (nfd < 0)
        break;
      if (nfd != 0) {
        processEvents(evnt, nfd);
        std::cout << nfd << std::endl;
      }
      yieldQueue(10);  // yielding queue before batch consumption;
      consumeBatch(8);
      yieldQueue(10);  // yielding queue after batch consumption;
      getWakeTime(&exit);
    };
    fdPool.release();
    cleanQueue();
    threadPool.threadfree();
    return 0;
  };

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
   int processEvents(struct epoll_event evnts[], int len) {
    for (int i = 0; i < len; i++) {
      auto handle = (buffioFd*)evnts[i].data.ptr;

      /*
       * below condition true if the fd is ready,
       * to read. if no request available the the
       * fd is just maked ready for read.
       */
      if (evnts[i].events & EPOLLIN) {
        auto req = handle->getReadReq();
        *handle | BUFFIO_READ_READY;
        if (req != nullptr) {
          if (evnts[i].events & EPOLLET) {
            requestBatch.push(req);
          } else {
            consumeEntry(req);
          };
        };
      }

      if (evnts[i].events & EPOLLOUT) {
        auto req = handle->getWriteReq();
        *handle | BUFFIO_WRITE_READY;
        std::cout << "we waked ready for write " << handle->getFd()
                  << std::endl;

        if (req != nullptr) {
          if (evnts[i].events & EPOLLET) {
            requestBatch.push(req);
          } else {
            consumeEntry(req);
          };
        };
      };
      auto req = handle->getReserveHeader();
      if (req->opCode == buffioOpCode::none)
        continue;

      // calls like accept and connect are handled here;
      switch (req->opCode) {
        case buffioOpCode::asyncConnect: {
          int code = -1;
          socklen_t len = sizeof(int);
          if (::getsockopt(req->reqToken.fd, SOL_SOCKET, SO_ERROR, &code,
                           &len) != 0) {
            auto handle = req->onAsyncDone
                              .onAsyncConnect(-1, nullptr, req->data.socketaddr)
                              .get();
            queue.push(handle);
            break;
          };
          auto handle = req->onAsyncDone
                            .onAsyncConnect(code, req->fd, req->data.socketaddr)
                            .get();
          queue.push(handle);
        } break;
        case buffioOpCode::asyncAcceptlocal: {
          sockaddr_un addr = {0};
          socklen_t len = sizeof(addr);
          int fd = ::accept(req->reqToken.fd, (sockaddr*)&addr, &len);
          auto handle = req->onAsyncDone.asyncAcceptlocal(fd, addr, len).get();
          queue.push(handle);
        } break;
        case buffioOpCode::asyncAcceptin: {
          sockaddr_in addrin = {0};
          socklen_t lenin = sizeof(addrin);
          int fd = ::accept(req->reqToken.fd, (sockaddr*)&addrin, &lenin);
          auto handle = req->onAsyncDone.asyncAcceptin(fd, addrin, lenin).get();
          queue.push(handle);

        }; break;
        case buffioOpCode::asyncAcceptin6: {
          sockaddr_in6 addr6 = {0};
          socklen_t len6 = sizeof(addr6);
          int fd = ::accept(req->reqToken.fd, (sockaddr*)&addr6, &len6);
          auto handle = req->onAsyncDone.asyncAcceptin6(fd, addr6, len6).get();
          queue.push(handle);

          break;
        };
        case buffioOpCode::waitAccept:
          [[fallthrough]];
        case buffioOpCode::waitConnect:
          queue.push(req->routine);
          break;
      };
    };
    return 0;
  };

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

  int dispatchHandle(int errorCode, buffioFd* fd, buffioHeader* header) {
    switch (header->opCode) {
      case buffioOpCode::read:
        [[fallthrough]];
      case buffioOpCode::readFile:
        queue.push(header->routine);
        break;
      case buffioOpCode::write:
        [[fallthrough]];
      case buffioOpCode::writeFile:
        queue.push(header->routine);
        break;
      case buffioOpCode::asyncRead:{
        fd->asyncReadDone(header);
        auto handle = header->onAsyncDone.onAsyncWrite(
            errorCode, header->data.buffer, header->len.len, fd, header).get();
        queue.push(handle);
      }break;
      case buffioOpCode::asyncWrite:{
        fd->asyncWriteDone(header);
        auto handle = header->onAsyncDone.onAsyncRead(
            errorCode, header->data.buffer, header->len.len, fd, header).get();
        queue.push(handle);
      } break;
      default:
        assert(false);
        break;
    };
    header->opCode = buffioOpCode::done;
    return 0;
  };


  /**
   * @brief consumeEntry is used to process the events received from the epoll and that are not edge-triggered.
   *
   * @param [in] req  pointer to the request Header that need to be processed.
   *
   * @return 0 on an entry is done,
   * @returns 1 if the entry is not fully consumed;
   * @returns -1 if there error occured and errno is set.
   *
   */

  int consumeEntry(buffioHeader* req) {
    // consumeBatch only handle read and write requests;
    ssize_t buffiolen = -1;

    switch (req->rwtype) {
      case buffioReadWriteType::read:
        buffiolen = ::read(req->reqToken.fd, req->bufferCursor, req->reserved);
        [[fallthrough]];
      case buffioReadWriteType::write:
        buffiolen = ::write(req->reqToken.fd, req->bufferCursor, req->reserved);
        [[fallthrough]];
      case buffioReadWriteType::rwEnd:
        if (buffiolen > 0) {
          req->bufferCursor +=
              buffiolen;  // moving the buffer to the next bytes;
          req->reserved -= buffiolen;
          if (req->reserved <= 0) {
            req->len.len = (req->bufferCursor - req->data.buffer);
            requestBatch.pop();
            dispatchHandle(0, req->fd, req);
            return 0;
          };
        };
        break;
      case buffioReadWriteType::recvfrom:
        [[fallthrough]];
      case buffioReadWriteType::recv:
        break;
      case buffioReadWriteType::sendto:
        [[fallthrough]];
      case buffioReadWriteType::send:
        break;
      default:
        dispatchHandle(-1, req->fd, req);
        break;
    };

    if (buffiolen < 0)
      return -1;
    return 1;
  };

   /**
   * @brief Executes a fixed number of fd events. only called when any fd is used with epoll edge-triggered
   *
   * Used to prevent starvation and improve fairness
   * between I/O and compute tasks.
   *
   * @param[in] batchSize Maximum number of tasks to execute
   */
  int consumeBatch(int cycle = 8) {
    if (requestBatch.empty())
      return -1;
    // consumeBatch only handle read and write requests;
    ssize_t buffiolen = -1;

    for (int i = 0; i < cycle; i++) {
      auto req = requestBatch.get();
      int error = consumeEntry(req);
      if (error == 1)
        goto continueloop;
      if (error == 0) {
        requestBatch.pop();
        goto continueloop;
      };

      // returned when there no data to read or write and we have consumed the
      // batch && bufferlen = -1
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        req->fd->unsetBit(req->unsetBit);
        requestBatch.pop();
        dispatchHandle(0, req->fd, req);
        goto continueloop;
      } else {
        dispatchHandle(errno, req->fd, req);
      };

    continueloop:
      if (requestBatch.empty())
        break;
      requestBatch.mvNext();
    };
    return 0;
  };

  /**@breief cleans the execution queue task and free up the memory.
  *
  */
  void cleanQueue() {
    while (!queue.empty()) {
      auto handle = queue.get();
      if (handle->waiter)
        handle->waiter->current.destroy();
      handle->current.destroy();
    };
  };

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
  int getWakeTime(bool* flag) {
    uint64_t startTime = timerClock.now();
    int looptime = timerClock.getNext(startTime);
    while (looptime == 0) {
      auto clkWork = timerClock.get();
      auto* promise = getPromise<char>(clkWork);
      promise->setStatus(buffioRoutineStatus::executing);
      queue.push(clkWork);
      looptime = timerClock.getNext(startTime);
    };
    if (!queue.empty())
      return 0;
    if (queue.empty() && timerClock.empty() && fdPool.empty()) {
      *flag = true;
      return 0;
    };
    if (timerClock.empty())
      return -1;
    return looptime;
  };
 /**
  * @brief method to execute the coroutines.
  *
  * yieldQueue method is used to the task if there's any, and it execute tasks in chunks.
  * means if the chunk is 10, it try to execute 10 tasks, it there task less than 10, 
  * then the task is executed in a cyclic way, and based on the status of the task and queue,
  * the yieldQueue returns.
  *
  * it returns only when on of the following happen,
  *  1) while executing the queue become empty.
  *  2) The cycle is finished
  *  3) Some internal error occured.
  *
  * @param[in] chunk the number of tasks to process in one shot.
  * @return return value less than 0 must be treated as error, and must be taken care of.
  */

  int yieldQueue(int chunk) {
    if (queue.empty())
      return -1;

    for (int i = chunk; 0 < i; i--) {
      auto handle = queue.get();
      auto promise = getPromise<char>(handle->current);
      if (promise->checkStatus() == buffioRoutineStatus::executing)
        handle->current.resume();

      switch (promise->checkStatus()) {
        case buffioRoutineStatus::executing:
          break;
        case buffioRoutineStatus::yield: {
          promise->setStatus(buffioRoutineStatus::executing);
        } break;
        case buffioRoutineStatus::waitingFd: {
          queue.erase();
        } break;
        case buffioRoutineStatus::paused: {
        } break;
        case buffioRoutineStatus::waiting: {
          queue.push(promise->getChild(), handle);
          queue.erase();
        } break;
        case buffioRoutineStatus::waitingTimer: {
          handle->current = nullptr;
          queue.pop();
        } break;
        case buffioRoutineStatus::pushTask: {
        } break;
        case buffioRoutineStatus::unhandledException:
          [[fallthrough]];

        case buffioRoutineStatus::error:
          [[fallthrough]];

        case buffioRoutineStatus::wakeParent: {
          if (handle->waiter != nullptr) {
            auto promiseP = getPromise<char>(handle->waiter->current);
            promiseP->setStatus(buffioRoutineStatus::executing);
            queue.push(handle->waiter);
            promise->setStatus(buffioRoutineStatus::paused);
            break;
          };
        }

          [[fallthrough]];
        case buffioRoutineStatus::zombie:
          [[fallthrough]];

        case buffioRoutineStatus::done: {
          handle->current.destroy();
          handle->current = nullptr;
          queue.pop();
          break;
        }
      };
      if (queue.empty())
        break;
      queue.mvNext();
    };
    return 0;
  };

  /**
   * @brief used to push routine to the execution queue before calling the run.
   *
   * @param[in] handle The handle to the routine.
   *
   * @return returns the value returned by the queue push method. and a value smaller than 0 must be treated as error.
  *
  */
  int push(buffioPromiseHandle handle) {
    auto* promise = getPromise<char>(handle);
    promise->setInstance(&timerClock, &poller, &fdPool, &headerPool);
    return queue.push(handle);
  };

 private:
  /*
   * sycPipe is used to ping the event loop that the worker thread have completed the work,
   * and the eventloop can dequeue the qeueue and consume the entry.
   *
   * The read side of the pipe is polled in event loop with header opCode dequeueThread.
   * and the write side of the pipe is sent to the worker thread to write to ping the evloop.
  */
  buffioFd* syncPipe;
  /*
  * poller is the instance of sockbroker that contains the code for worker and creation of queue, that the
  * worker thread and the eventloop can use to transfer task and receive task.
  *
  */
  buffioSockBroker poller;
  /*
  * timerClock is a class that implements all the time releated request needed by the tasks,
  */

  buffioClock timerClock;

  /*
  * fdPool is the pool that have all tha fd that are currently being used and every fd after being used, 
  * the fd must be returned to the pool, via the appropriate call.
  */
  buffioFdPool fdPool;

  /*
  * queue is a circular linked list that is used execute tasks.
  *
  */
  buffioQueue<> queue;
  /*
  * headerPool is the pool of header that the routine can used to get header, to prevent from alocating new one,
  * and after using a header it must be returned to the headerpool, via appropriate call or method provided.
  *
  */
  buffioMemoryPool<buffioHeader> headerPool;
  /*
  * requestBatch is the circular linked list of requests that need read/write, requestBatch is mostly used if edge-triggered
  * is used in epoll.
  */
  buffioQueue<buffioHeader, void*, buffioQueueNoMem> requestBatch;
  /*
  * threadPool is the instanceof buffiothread is used to create thread,
  * but it doesn't manages the thread destruction, it must be managed by the porgrammer.
  *
  */
  buffioThread threadPool;
};

#endif
