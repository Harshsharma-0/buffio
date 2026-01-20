#ifndef __BUFFIO_THREAD_HPP__
#define __BUFFIO_THREAD_HPP__

/*
 * Error codes range reserved for buffiothread
 *  [6000 - 7500]
 *  6000 <= errorcode <= 7500
 */

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unordered_map>
#include <vector>

// The struct buffiothreadinfo lifecycle after running the thread is upon user if they want to keep it or not
// but when running thread struct buffiothreadinfo must be valid

// class to handle signals across many buffiothreads instance;
// order in which the handler are added, in that specific order the handler are executed;
// FIFO: policy
class buffioSignalHandler {
    struct handler {
        int id;
        int (*func)(void* data);
        void* data;
        struct handler* next;
    };

    using buffioSignalMap = std::unordered_map<int, std::vector<struct handler>>;

public:
    buffioSignalHandler()
        : mapLock(0)
    {
        ::sigemptyset(&signalSet);
        map.reserve(5);
    }
    static void* threadSigHandler(void* data)
    {
        buffioSignalHandler* core = reinterpret_cast<buffioSignalHandler*>(data);
        int s = 0, sig = 0;
        sigset_t mineSig = core->signalSet;
        buffioSignalMap* cmap = &core->map;

        for (;;) {
            s = ::sigwait(&mineSig, &sig);
            if ((*cmap).find(sig) != (*cmap).end()) {
                auto& vec = (*cmap)[sig];
                for (auto& ent : vec) {
                    ent.func(ent.data);
                }
            }
        }
        return nullptr;
    };
    buffioSignalHandler(const buffioSignalHandler&) = delete;
    // function to add a mask
    int maskadd(int sigNum = 0)
    {
        if (sigNum == 0)
            return -1;
        return ::sigaddset(&signalSet, sigNum);
    };
    // function remove the mask
    int maskremove(int sigNum = 0)
    {
        if (sigNum == 0)
            return -1;
        return ::sigdelset(&signalSet, sigNum);
    };

    // function to mount the handler
    int mount()
    {
        if (::pthread_sigmask(SIG_BLOCK, &signalSet, NULL) != 0)
            return -1;
        if (::pthread_create(&threadHandler, NULL,
                buffioSignalHandler::threadSigHandler, this) != 0)
            return -1;
        return 0;
    };
    // function to unregister a mask
    int unregister(int sigNum = 0,int id = -1)
    {
        if (sigNum == 0 || id = -1)
            return -1;
        map.erase(sigNum); // don't care if exist or not
        return 0;
    };

    int registerHandler(int sigNum = 0,
        int (*func)(void* data) = nullptr,
        void* data = nullptr)
    {

        if (sigNum == 0 || func == nullptr)
            return -1;
        map[sigNum].push_back({ .func = func, .data = data });
        return 0;
    };

private:
    std::atomic<int> mapLock; // 0 if unlocked, -1 if locked;
    buffioSignalMap map;
    sigset_t signalSet;
    pthread_t threadHandler;
};

// thread termination is the user work
class buffiothread {

    struct threadinternal {
        void* resource;
        void* stack;
        char* name;
        int (*func)(void*);
        threadinternal* next;
        size_t stackSize;
        pthread_attr_t attr;
        pthread_t id;
        std::atomic<buffioThreadStatus> status;
    };

public:
    buffiothread()
        : threads(nullptr)
        , numThreads(0)
        , threadsId(nullptr){}

    // only free the allocated resource for the thread not terminate it
    void threadfree()
    {
        if (threadsId != nullptr) delete[] threadsId;
        
        struct threadinternal *tmp = nullptr;
        for (auto* loop = threads; loop != nullptr;) {
            if (loop->name != nullptr)
                delete[] loop->name;
            ::free(loop->stack);
          tmp = loop;
          loop = loop->next;
          delete tmp;
        }
        numThreads = 0;
        threads = threadsId = nullptr;
        return;
    };

    int add(const char* name = nullptr,
        int (*func)(void*) = nullptr,
        void* data = nullptr,
        size_t stackSize = buffiothread::SD)
    {

        if (stackSize < buffiothread::S1KB || func == nullptr)
            return -1;
        struct threadinternal* tmpThr = new struct threadinternal;
        std::memset(tmpThr, '\0', sizeof(struct threadinternal));
        tmpThr->status = buffioThreadStatus::configOk;

        if (::pthread_attr_init(&tmpThr->attr) != 0)
            return -1;

        tmpThr->resource = data;
        tmpThr->name = nullptr;
        tmpThr->stackSize = stackSize;
        if (name != nullptr) {
            size_t len = std::strlen(name);
            if (len < 100) {
                // TODO: error check;
                char* name_tmp = new char[len + 1];
                std::memcpy(name_tmp, name, len + 1);
                tmpThr->name = name_tmp;
            }
        }

        if (::posix_memalign(&tmpThr->stack, sysconf(_SC_PAGESIZE), stackSize) != 0)
            return -1;
        if (::pthread_attr_setstack(&tmpThr->attr, tmpThr->stack, stackSize) != 0)
            return -1;

        tmpThr->stackSize = stackSize;
        tmpThr->func = func;
        if (threads == nullptr) {
            threads = tmpThr;
        } else {
            tmpThr->next = threads;
            threads = tmpThr;
        }
        tmpThr->status = buffioThreadStatus::configOk;
        numThreads += 1;
        return 0;
    };

    // return array of ids of the thread
    [[nodiscard]]
    pthread_t* run(){

        if (threadsId != nullptr || threads == nullptr)
            return nullptr;

        threadsId = new pthread_t[numThreads];
        auto tmpStatus = buffioThreadStatus::configOk;
        size_t count = 0;

        for (auto* thr = threads; threads != nullptr; threads = threads->next) {

            if (thr->status != buffioThreadStatus::configOk) continue;
            if (::pthread_create(&thr->id, &thr->attr, buffioFunc, thr) != 0)
                return nullptr;

            ::pthread_attr_destroy(&thr->attr);
            threadsId[count] = thr->id;
            count += 1;
        }
        return threadsId;
    };

    void wait(pthread_t threadId){
        ::pthread_join(threadId, NULL);
    }

    static int setname(const char* name){
        return prctl(PR_SET_NAME, name, 0, 0, 0);
    };
    buffiothread(const buffiothread&) = delete;

    static constexpr size_t S1MB = 1024 * 1024;
    static constexpr size_t S4MB = 4 * (1024 * 1024);
    static constexpr size_t S9MB = 9 * (1024 * 1024);
    static constexpr size_t S10MB = 10 * (1024 * 1024);
    static constexpr size_t SD = buffiothread::S9MB;
    static constexpr size_t S1KB = 1024;

private:
    static void* buffioFunc(void* data)
    {

        struct threadinternal* tmpThr = reinterpret_cast<threadinternal*>(data);
        tmpThr->status.store(buffioThreadStatus::running, std::memory_order_release);

        if (tmpThr->name != nullptr)
            ::prctl(PR_SET_NAME, tmpThr->name, 0, 0, 0);


        int error = tmpThr->func(tmpThr->resource);

        tmpThr->status.store(buffioThreadStatus::done,
            std::memory_order_release);
        ::pthread_exit(nullptr);
        return nullptr;
    };

    struct threadinternal* threads;
    pthread_t* threadsId;
    size_t numThreads;
};

#endif
