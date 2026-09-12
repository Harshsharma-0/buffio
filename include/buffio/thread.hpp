#ifndef BUFFIO_THREAD
#define BUFFIO_THREAD

#include "buffio/config.hpp"
#include "buffio/ecode.hpp"
#include "buffio/macro.hpp"
#include <atomic>
#include <version>

#ifdef BUFFIO_OS_LINUX
#include <semaphore.h>
#endif

#ifdef __cpp_lib_latch
#include <latch>
#endif

namespace buffio {

#undef __cpp_lib_latch
#ifdef __cpp_lib_latch
using Latch = std::latch;
#else

// TODO : Define error codes
class Latch {
private:
  std::atomic<std::ptrdiff_t> count = 0;

public:
  Latch(std::ptrdiff_t cnt) : count(cnt) {};
  ~Latch() = default;

  void count_down(std::ptrdiff_t n = 1);
  bool try_wait() const { return count.load(std::memory_order_acquire) > 0; };
  void arrive_and_wait(std::ptrdiff_t n = 1);
  void wait();
};

#endif


class Signal {
private:
  std::atomic<uint32_t> work_count = 0;
  std::atomic<uint32_t> sleeping_count = 0;

public:
  void post(uint32_t n);
  void wait();
};

using WorkerSignal = buffio::Signal;

using threadFuncSig = void (*)(void *);

class thread {
public:
  
  /* TODO: add timeout while joining the thread */
  thread() : routine(nullptr), args(nullptr) {};
  int run(threadFuncSig start, void *args);

  void *getReturnValue() const {return rval; };
  int join(uint32_t timeout = -1);

private:
  BUFFIO_OS_INSERT(pthread_t, pthread_t, HANDLE)
  handle;
  threadFuncSig routine;
  void *args;
  void *rval;
};

class semaphore {
public:
  semaphore &operator=(semaphore const &val) = delete;
  semaphore() {};

  int create(size_t initialValue);
  int post();
  int wait();
  void destroy();

private:
  BUFFIO_OS_INSERT(sem_t, sem_t, HANDLE)
  lsem;
};

}; // namespace buffio
#endif
