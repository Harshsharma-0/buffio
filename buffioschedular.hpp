#ifndef BUFFIO_SCHEDULAR
#define BUFFIO_SCHEDULAR

#include "buffioQueue.hpp"
#include "buffioclock.hpp"

class buffioScheduler {

public:
    // constructor overload.
    buffioScheduler()
        : syncPipe(nullptr)
    {
        fdPool.mountPool(&headerPool);
    };
    ~buffioScheduler() = default;

    int run()
    {

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
         * batch of fd allocated to the main event loop to process, is just a circular list,
         * as we support EPOLLET edge-triggered mode, then we need batch procssing.
         */
        buffioHeader* batch = nullptr;
        size_t batchCount = 0;
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
                consumeBatch();
                getWakeTime(&exit);
            };
            int error = yieldQueue(10);
        };
        fdPool.release();
        cleanQueue();
        threadPool.threadfree();
        return 0;
    };

    int processEvents(struct epoll_event evnts[], int len)
    {
        for (int i = 0; i < len; i++) {
            auto handle = (buffioFd*)evnts[i].data.ptr;

            /*
             * below condition true if the fd is ready,
             * to read. if no request available the the
             * fd is just maked ready for read.
             */
            if (evnts[i].events & EPOLLIN) {
                auto req = handle->getReadReq();
                if (req == nullptr) {
                    *handle | BUFFIO_READ_READY;
                } else {
                    req->reserved = 0;
                    requestBatch.push(req);
                };
            };
            if (evnts[i].events & EPOLLOUT) {
                auto req = handle->getWriteReq();
                if (req == nullptr) {
                    *handle | BUFFIO_WRITE_READY;
                } else {
                    req->reserved = req->len.len;
                    requestBatch.push(req);
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
                if (::getsockopt(req->reqToken.fd, SOL_SOCKET, SO_ERROR, &code, &len) != 0) {
                    auto handle = req->onAsyncDone.onAsyncConnect(-1, nullptr, req->data.socketaddr).get();
                    queue.push(handle);
                    break;
                };
                auto handle = req->onAsyncDone.onAsyncConnect(code, req->fd, req->data.socketaddr).get();
                queue.push(handle);
            } break;
            case buffioOpCode::asyncAccept: {
                int fd = ::accept(req->reqToken.fd, req->data.socketaddr,
                    (socklen_t*)&req->len.socklen);
                auto handle = req->onAsyncDone.onAsyncAccept(fd, req->data.socketaddr).get();
                queue.push(handle);
            } break;
            case buffioOpCode::waitAccept:
                [[fallthrough]];
            case buffioOpCode::waitConnect:
                queue.push(req->routine);
                break;
            };
        };
        return 0;
    };

   /* helper funcation to dispatch the routine after a successfull/errored read/write
    *  on the file descriptor.
    */
   
    int dispatchHandle(int errorCode,buffioHeader *header){
    };
    int consumeBatch(int cycle = 8)
    {
        // consumeBatch only handle read and write requests;
        ssize_t err = -1;
        for (int i = 0; i < cycle; i++){
            auto req = requestBatch.get();
            switch(req->rwtype){
            case buffioReadWriteType::read:
              err = ::read(req->reqToken.fd,req->data.buffer,req->reserved);
            break;
            case buffioReadWriteType::write:{
              err = ::write(req->reqToken.fd,req->data.buffer,req->len.len);  
            };
            break;
            case buffioReadWriteType::recvfrom:
            break;
            case buffioReadWriteType::recv:
            break;
            case buffioReadWriteType::sendto:
            break;
            case buffioReadWriteType::send:
            break;
            };
           switch(err){
            default:
            break;
           }
          if(requestBatch.empty()) break;
            requestBatch.mvNext();
        };
        return 0;
    };
    void cleanQueue()
    {
        while (!queue.empty()) {
            auto handle = queue.get();
            if (handle->waiter)
                handle->waiter->current.destroy();
            handle->current.destroy();
        };
    };

    int getWakeTime(bool* flag)
    {
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
        if (queue.empty() && timerClock.empty() && !poller.busy()) {
            *flag = true;
            return 0;
        };
        return looptime;
    };

    int yieldQueue(int chunk)
    {
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

    int push(buffioPromiseHandle handle)
    {
        auto* promise = getPromise<char>(handle);
        promise->setInstance(&timerClock, &poller, &fdPool, &headerPool);
        return queue.push(handle);
    };

private:
    buffioFd* syncPipe;
    buffioSockBroker poller;
    buffioClock timerClock;
    buffioFdPool fdPool;
    buffioQueue<> queue;
    buffioMemoryPool<buffioHeader> headerPool;
    buffioQueue<buffioHeader, void*, buffioQueueNoMem> requestBatch;
    buffioThread threadPool;
};

#endif
