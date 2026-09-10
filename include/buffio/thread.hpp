#ifndef BUFFIO_THREAD
#define BUFFIO_THREAD

#include "buffio/config.hpp"
#include "buffio/macro.hpp"
#include <atomic>

#if defined(BUFFIO_OS_LINUX) || defined(BUFFIO_OS_BSD)
#include <pthread.h>
#include <semaphore.h>
#elif defined(BUFFIO_OS_WINDOWS)
 #pragma comment(lib, "Synchronization.lib")
 
#endif

#ifdef __cpp_lib_latch
#include <latch>
#endif

namespace buffio
{

  class Signal
  {
  private:
    std::atomic<uint32_t> work_count = 0;
    std::atomic<uint32_t> sleeping_count = 0;

  public:
    template <typename AtomicType>
    static inline int Signal_wait(std::atomic<AtomicType> *addr, AtomicType expected)
    {
#if defined(BUFFIO_OS_LINUX)
      return syscall(SYS_futex, (AtomicType *)addr, FUTEX_WAIT_PRIVATE, expected, NULL, NULL, 0);

#elif defined(BUFFIO_OS_WINDOWS)
      WaitOnAddress(addr, &expected, sizeof(AtomicType), INFINITE);
      return 0;
#endif
    };

    template <typename AtomicTypeW>
    static inline int Signal_wake(std::atomic<AtomicTypeW> *addr, AtomicTypeW num_threads)
    {
#if defined(BUFFIO_OS_LINUX)
      return syscall(SYS_futex, (AtomicTypeW *)addr, FUTEX_WAKE_PRIVATE, num_threads);
#elif defined(BUFFIO_OS_WINDOWS)
      // num_threads ignored on windows;

      while(num_threads > 0){ 
        WakeByAddressSingle(addr);
        num_threads -= 1;
      }
      return 0;
#endif
    };

    void post(uint32_t n)
    {
      work_count.fetch_add(n, std::memory_order_release);
      uint32_t inactive = sleeping_count.load(std::memory_order_acquire);
      uint32_t minWake = inactive > n ? n : inactive;

#if defined(BUFFIO_OS_LINUX)
      Signal::Signal_wake<uint32_t>(&work_count, (uint32_t)minWake);

#elif defined(BUFFIO_OS_WINDOWS)
      while (minWake != 0)
      {
        Signal::Signal_wake<uint32_t>(&work_count, 0);
        --minWake;
      };
#endif
    };

    void wait()
    {

      for (;;)
      {
        uint32_t w_cnt = work_count.load(std::memory_order_acquire);

        /* try acquiring a slot in the work */
        while (w_cnt > 0)
        {
          if (work_count.compare_exchange_weak(
                  w_cnt, w_cnt - 1,
                  std::memory_order_acquire,
                  std::memory_order_acquire))
          {
            return;
          }
        };

        /* announce that we are going to be sleeping */
        sleeping_count.fetch_add(1, std::memory_order_relaxed);

        // checking for work once more
        w_cnt = work_count.load(std::memory_order_acquire);

        if (w_cnt != 0)
        {
          sleeping_count.fetch_sub(1, std::memory_order_relaxed);
          continue;
        };

        Signal::Signal_wait<uint32_t>(&work_count, 0);
        /* code to wait */
        sleeping_count.fetch_sub(1, std::memory_order_relaxed);
      };
    };
  };

  using WorkerSignal = buffio::Signal;

#ifdef __cpp_lib_latch
  class Latch : public std::latch
  {
  };
#else

  class Latch
  {
    private: 
    std::atomic<std::ptrdiff_t> count = 0;

    public:
    Latch(std::ptrdiff_t cnt) : count(cnt) {};
    ~Latch() = default;
    void count_down(std::ptrdiff_t n = 1)
    {
      
      std::ptrdiff_t val = count.load(std::memory_order_acquire);
      while(val > 0){
        if(count.compare_exchange_weak(val,val - n,std::memory_order_acq_rel)) break;
      };

      if((val - n) <= 0){
      #ifdef BUFFIO_OS_LINUX
        buffio::Signal::Signal_wake<std::ptrdiff_t>(&count,PTRDIFF_MAX);
      #else
       WakeByAddressAll(&count);
      #endif
      };

    };

    bool try_wait() const { return count.load(std::memory_order_acquire) > 0; };

    void arrive_and_wait(std::ptrdiff_t n = 1)
    {
      count_down(n);
      wait();
    }

    void wait()
    {

      for (;;)
      {
        std::ptrdiff_t cnt = count.load(std::memory_order_acquire);
        if (cnt <= 0)
          return;
        Signal::Signal_wait<std::ptrdiff_t>(&count, cnt);
      };
    };
  };

#endif

  using threadFuncSig = void (*)(void *);

  class thread
  {
  public:
    thread() : routine(nullptr), args(nullptr) {};

    int run(threadFuncSig start, void *args);
    int join();

  private:
    BUFFIO_OS_INSERT(pthread_t, pthread_t, HANDLE)
    handle;
    threadFuncSig routine;
    void *args;
  };

  class semaphore
  {
  public:
    semaphore &operator=(semaphore const &val) { return *this; }
    semaphore() {};
    int create(size_t initialValue)
    {
      BUFFIO_LIN_INSERT(if (sem_init(&lsem, 0, initialValue) == 0) return 0;)

      BUFFIO_WIN_INSERT(
          LONG lmaxCount = static_cast<LONG>(initialValue);
          HANDLE semHandle = CreateSemaphoreA(NULL, lmaxCount, lmaxCount, NULL);
          lsem = semHandle;
          if (semHandle != NULL) return 0;)
      /* HERE ONLY IF THERE IS ERROR CREATING SEMAPHORE */

      return -1;
    };
    int post()
    {
      BUFFIO_LIN_INSERT(sem_post(&lsem));
      BUFFIO_WIN_INSERT(
          LONG val = 0;
          ReleaseSemaphore(lsem, 1, &val);)
      return 0;
    };

    int wait()
    {
      BUFFIO_LIN_INSERT(sem_wait(&lsem);)
      BUFFIO_WIN_INSERT(
          LONG val = 0;
          WaitForSingleObject(lsem, -1);)
      return 0;
    };
    void destroy()
    {
      BUFFIO_LIN_INSERT(sem_destroy(&lsem);)
      BUFFIO_WIN_INSERT(CloseHandle(lsem);)
    };

  private:
    BUFFIO_OS_INSERT(sem_t, sem_t, HANDLE)
    lsem;
  };

};
#endif
