#include "buffio/thread.hpp"

#ifdef BUFFIO_OS_WINDOWS

template <typename AtomicType>
static inline int Signal_wait(std::atomic<AtomicType> *addr,
                              AtomicType expected) {
  WaitOnAddress(addr, &expected, sizeof(AtomicType), INFINITE);
  return 0;
};

template <typename AtomicTypeW>
static inline int Signal_wake(std::atomic<AtomicTypeW> *addr,
                              AtomicTypeW num_threads, bool all = false) {
  if (all) {
    WakeByAddressAll(addr);
    return;
  };

  while (num_threads > 0) {
    WakeByAddressSingle(addr);
    num_threads -= 1;
  }
  return 0;
};

#elifdef BUFFIO_OS_LINUX
#include <linux/futex.h>
#include <syscall.h>
#include <unistd.h>

template <typename AtomicType>
static inline int Signal_wait(std::atomic<AtomicType> *addr,
                              AtomicType expected) {
  return syscall(SYS_futex, (AtomicType *)addr, FUTEX_WAIT_PRIVATE, expected,
                 NULL, NULL, 0);
};

template <typename AtomicTypeW>
static inline int Signal_wake(std::atomic<AtomicTypeW> *addr,
                              AtomicTypeW num_threads, bool all = false) {
  return syscall(SYS_futex, (AtomicTypeW *)addr, FUTEX_WAKE_PRIVATE,
                 num_threads);
};

#endif

void buffio::Signal::post(uint32_t n) {
  work_count.fetch_add(n, std::memory_order_release);
  uint32_t inactive = sleeping_count.load(std::memory_order_acquire);
  uint32_t minWake = inactive > n ? n : inactive;

  Signal_wake<uint32_t>(&work_count, (uint32_t)minWake);
};

void buffio::Signal::wait() {

  for (;;) {
    uint32_t w_cnt = work_count.load(std::memory_order_acquire);

    /* try acquiring a slot in the work */
    while (w_cnt > 0) {
      if (work_count.compare_exchange_weak(w_cnt, w_cnt - 1,
                                           std::memory_order_acquire,
                                           std::memory_order_acquire)) {
        return;
      }
    };

    /* announce that we are going to be sleeping */
    sleeping_count.fetch_add(1, std::memory_order_relaxed);

    // checking for work once more
    w_cnt = work_count.load(std::memory_order_acquire);

    if (w_cnt != 0) {
      sleeping_count.fetch_sub(1, std::memory_order_relaxed);
      continue;
    };

    Signal_wait<uint32_t>(&work_count, 0);
    /* code to wait */
    sleeping_count.fetch_sub(1, std::memory_order_relaxed);
  };
};

#ifndef __cpp_lib_latch

void buffio::Latch::count_down(std::ptrdiff_t n) {

  std::ptrdiff_t val = count.load(std::memory_order_acquire);
  while (val > 0) {
    if (count.compare_exchange_weak(val, val - n, std::memory_order_acq_rel))
      break;
  };

  if ((val - n) <= 0) {
    Signal_wake<std::ptrdiff_t>(&count, PTRDIFF_MAX, true);
  };
};

void buffio::Latch::arrive_and_wait(std::ptrdiff_t n) {
  count_down(n);
  wait();
}

void buffio::Latch::wait() {
  for (;;) {
    std::ptrdiff_t cnt = count.load(std::memory_order_acquire);
    if (cnt <= 0)
      return;
    Signal_wait<std::ptrdiff_t>(&count, cnt);
  };
};

#endif
