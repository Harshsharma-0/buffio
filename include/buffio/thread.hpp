#ifndef __BUFFIO_THREAD_HPP__
#define __BUFFIO_THREAD_HPP__

/*
 * Error codes range reserved for buffiothread
 *  [6000 - 7500]
 *  6000 <= errorcode <= 7500
 */

#include "enum.hpp"
#include <atomic>
#include <cassert>
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
    return 0;
  };
  // function to unregister a mask
  int unregister(int sigNum = 0, int id = -1) {
    if (sigNum == 0 || id == -1)
      return -1;

    //    map.erase(sigNum); // don't care if exist or not
    return 0;
  };

  int registerHandler(int sigNum, int (*func)(void *data), void *data,
                      bool exitAfterSignal, int id) {

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

namespace buffio {
class thread {
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
  thread();
  thread(thread const &) = delete;
  thread(thread &&) = delete;
  thread &operator=(thread const &) = delete;
  thread &operator=(thread &&) = delete;

  ~thread() = default;
  // only free the allocated resource for the thread not terminate it
  void free();
  int run(const char *name, int (*func)(void *), void *data,
          size_t stackSize = buffio::thread::SD);

  void wait(pthread_t threadId) { ::pthread_join(threadId, NULL); }
  size_t num() const { return numThreads.load(std::memory_order_acquire); }
  static int setname(const char *name) {
    return ::prctl(PR_SET_NAME, name, 0, 0, 0);
  };
  static constexpr size_t S1MB = 1024 * 1024;
  static constexpr size_t S4MB = 4 * (1024 * 1024);
  static constexpr size_t S9MB = 9 * (1024 * 1024);
  static constexpr size_t S10MB = 10 * (1024 * 1024);
  static constexpr size_t SD = buffio::thread::S9MB;
  static constexpr size_t S1KB = 1024;

private:
  static void *buffioFunc(void *data);

  struct threadinternal *threads;
  pthread_mutex_t buffioMutex;
  bool mutexEnabled;
  std::atomic<size_t> numThreads;
};

}; // namespace buffio

/*
class buffioThreadView {
public:
  buffioThreadView() = default;
  ~buffioThreadView() = default;

  void make() {
    if (localInstance.use_count() != 0)
      return;
    try {
      localInstance = std::make_shared<buffioThread>();
    } catch (std::exception &e) {
      std::cout << e.what() << std::endl;
    };
    return;
  };
  buffioThreadView &operator=(buffioThreadView const &view) {
    if (view.localInstance.use_count() == 0)
      return *this;
    localInstance = view.localInstance;
    return *this;
  };
  std::weak_ptr<buffioThread> get() const { return localInstance; };

private:
  std::shared_ptr<buffioThread> localInstance;
};
*/
#endif
