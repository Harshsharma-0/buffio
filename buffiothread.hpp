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
#include <memory>
#include <new>
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

/*
 class to handle signals across many buffiothreads instance;
 the first handler added for specific signal is invoked in end if another
 signal handler is added for that same signal. the last one added is invoked
 first.
*/
class buffioSignalHandler {
  struct handler {
    int id;
    bool exitAfterSignal;
    int (*func)(void *data);
    void *data;
    std::unique_ptr<struct handler> next;
  };

  using buffioSignalMap =
      std::unordered_map<int, std::unique_ptr<struct handler>>;

public:
  buffioSignalHandler() : mapLock(0) {
    ::sigemptyset(&signalSet);
    map.reserve(5);
  }
  static void *threadSigHandler(void *data) {
    buffioSignalHandler *core = reinterpret_cast<buffioSignalHandler *>(data);
    int s = 0, sig = 0;
    sigset_t mineSig = core->signalSet;
    buffioSignalMap *cmap = &core->map;

    for (;;) {
      s = ::sigwait(&mineSig, &sig);
      if ((*cmap).find(sig) != (*cmap).end()) {
        auto vec = (*cmap)[sig].get();
        for (auto itr = vec; vec != nullptr; vec = vec->next.get()) {
          vec->func(vec->data);
        }
        break;
      }
    }
    // TODO: add custom signal handling;
    pthread_exit(nullptr);
  };

  buffioSignalHandler(const buffioSignalHandler &) = delete;
  // function to add a mask
  int maskadd(int sigNum = 0) {
    if (sigNum == 0)
      return -1;
    return ::sigaddset(&signalSet, sigNum);
  };
  // function remove the mask
  int maskremove(int sigNum = 0) {
    if (sigNum == 0)
      return -1;
    return ::sigdelset(&signalSet, sigNum);
  };

  // function to mount the handler
  int mount() {
    if (::pthread_sigmask(SIG_BLOCK, &signalSet, NULL) != 0)
      return -1;
    if (::pthread_create(&threadHandler, NULL,
                         buffioSignalHandler::threadSigHandler, this) != 0)
      return -1;
    pthread_join(threadHandler, NULL);
    return 0;
  };
  // function to unregister a mask
  int unregister(int sigNum = 0, int id = -1) {
    if (sigNum == 0 || id == -1)
      return -1;

    //    map.erase(sigNum); // don't care if exist or not
    return 0;
  };

  int registerHandler(int sigNum = 0, int (*func)(void *data) = nullptr,
                      void *data = nullptr, bool exitAfterSignal = true,
                      int id = 0) {

    if (sigNum == 0 || func == nullptr)
      return -1;

    auto tmpHandlerInfo = std::make_unique<struct handler>();
    if (tmpHandlerInfo.get() == nullptr)
      return -1;

    tmpHandlerInfo->id = id;
    tmpHandlerInfo->func = func;
    tmpHandlerInfo->data = data;
    tmpHandlerInfo->exitAfterSignal = exitAfterSignal;

    if (map.find(sigNum) == map.end()) {
      map[sigNum] = std::move(tmpHandlerInfo);
      return 0;
    }

    auto pulledData = std::move(map[sigNum]);
    tmpHandlerInfo->next = std::move(pulledData);
    map[sigNum] = std::move(tmpHandlerInfo);

    return 0;
  };

private:
  std::atomic<int> mapLock;
  buffioSignalMap map;
  sigset_t signalSet;
  pthread_t threadHandler;
};

// thread termination is the user work
class buffioThread {

  struct threadinternal {
    void *resource;
    void *stack;
    char *name;
    int (*func)(void *);
    threadinternal *next;
    size_t stackSize;
    pthread_attr_t attr;
    pthread_t id;
    std::atomic<buffioThreadStatus> status;
    std::atomic<size_t> *numThreads;
  };

public:
  buffioThread() : threads(nullptr), numThreads(0), mutexEnabled(false) {
    buffioMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutexattr_t mutexAttr;

    if (pthread_mutexattr_init(&mutexAttr) != 0)
      return;
    if (pthread_mutex_init(&buffioMutex, &mutexAttr) != 0)
      return;
    pthread_mutexattr_destroy(&mutexAttr);
    mutexEnabled = true;
  }
  buffioThread(buffioThread const &) = delete;
  buffioThread(buffioThread &&) = delete;
  buffioThread &operator=(buffioThread const &) = delete;
  buffioThread &operator=(buffioThread &&) = delete;

  ~buffioThread() { threadfree(); }
  // only free the allocated resource for the thread not terminate it
  void threadfree() {

    if (mutexEnabled == false)
      return;

    ::pthread_mutex_lock(&buffioMutex);
    struct threadinternal *tmp = nullptr;
    for (auto *loop = threads; loop != nullptr;) {
      if (loop->name != nullptr)
        delete[] loop->name;
      ::free(loop->stack);
      tmp = loop;
      loop = loop->next;
      delete tmp;
    }
    threads = nullptr;
    ::pthread_mutex_unlock(&buffioMutex);
    pthread_mutex_destroy(&buffioMutex);
    return;
  };

  int run(const char *name, int (*func)(void *), void *data,
          size_t stackSize = buffioThread::SD) {

    if (stackSize < buffioThread::S1KB || func == nullptr ||
        mutexEnabled == false)
      return -1;

    struct threadinternal *tmpThr = nullptr;
    try {
      tmpThr = new struct threadinternal;
    } catch (std::exception &e) {
      return -1;
    };

    ::memset(tmpThr, '\0', sizeof(struct threadinternal));
    tmpThr->status = buffioThreadStatus::configOk;

    if (::pthread_attr_init(&tmpThr->attr) != 0) {
      delete tmpThr;
      return -1;
    }

    tmpThr->resource = data;
    tmpThr->name = nullptr;
    tmpThr->stackSize = stackSize;
    tmpThr->numThreads = &numThreads;
    tmpThr->next = nullptr;
    if (name != nullptr) {
      size_t len = std::strlen(name);
      if (len < 100) {
        try {
          char *name_tmp = new char[len + 1];
          std::memcpy(name_tmp, name, len);
          tmpThr->name = name_tmp;
          name_tmp[len] = '\0';

        } catch (std::exception &e) {
          // whill run without name no error check needed
        };
      }
    }

    if (::posix_memalign(&tmpThr->stack, sysconf(_SC_PAGESIZE), stackSize) !=
        0) {
      if (tmpThr->name != nullptr)
        delete tmpThr->name;

      ::pthread_attr_destroy(&tmpThr->attr);
      delete tmpThr;
      return -1;
    }
    if (::pthread_attr_setstack(&tmpThr->attr, tmpThr->stack, stackSize) != 0) {
      if (tmpThr->name != nullptr)
        delete tmpThr->name;

      ::pthread_attr_destroy(&tmpThr->attr);
      ::free(tmpThr->stack);
      delete tmpThr;
      return -1;
    }

    tmpThr->status = buffioThreadStatus::configOk;
    tmpThr->stackSize = stackSize;
    tmpThr->func = func;

    if (::pthread_create(&tmpThr->id, &tmpThr->attr, buffioFunc, tmpThr) != 0) {
      if (tmpThr->name != nullptr)
        delete tmpThr->name;

      ::free(tmpThr->stack);
      ::pthread_attr_destroy(&tmpThr->attr);
      delete tmpThr;
      return -1;
    }

    ::pthread_mutex_lock(&buffioMutex);
    if (threads == nullptr) {
      threads = tmpThr;
    } else {
      tmpThr->next = threads;
      threads = tmpThr;
    }
    ::pthread_mutex_unlock(&buffioMutex);

    return 0;
  };

  void wait(pthread_t threadId) { ::pthread_join(threadId, NULL); }

  static int setname(const char *name) {
    return ::prctl(PR_SET_NAME, name, 0, 0, 0);
  };

  static constexpr size_t S1MB = 1024 * 1024;
  static constexpr size_t S4MB = 4 * (1024 * 1024);
  static constexpr size_t S9MB = 9 * (1024 * 1024);
  static constexpr size_t S10MB = 10 * (1024 * 1024);
  static constexpr size_t SD = buffioThread::S9MB;
  static constexpr size_t S1KB = 1024;

private:
  static void *buffioFunc(void *data) {

    struct threadinternal *tmpThr = reinterpret_cast<threadinternal *>(data);
    tmpThr->status.store(buffioThreadStatus::running,
                         std::memory_order_release);

    tmpThr->numThreads->fetch_add(1, std::memory_order_acq_rel);
    if (tmpThr->name != nullptr)
      ::prctl(PR_SET_NAME, tmpThr->name, 0, 0, 0);

    int mutexEnabled = tmpThr->func(tmpThr->resource);

    tmpThr->status.store(buffioThreadStatus::done, std::memory_order_release);
    tmpThr->numThreads->fetch_add(-1, std::memory_order_acq_rel);

    ::pthread_exit(nullptr);
    return nullptr;
  };

  struct threadinternal *threads;
  pthread_mutex_t buffioMutex;
  bool mutexEnabled;
  std::atomic<size_t> numThreads;
};

class buffioThreadView {
public:
  buffioThreadView() = default;
  ~buffioThreadView() = default;

  void make() {
    if (localInstance.use_count() == 0)
      return;
    localInstance = std::make_shared<buffioThread>();
    return;
  };
  buffioThreadView &operator=(buffioThreadView const &view) {
    if (view.localInstance.use_count() == 0)
      return *this;
    localInstance = view.localInstance;
    return *this;
  };
  buffioThread *operator()() const { return localInstance.get(); };

private:
  std::shared_ptr<buffioThread> localInstance;
};
#endif
