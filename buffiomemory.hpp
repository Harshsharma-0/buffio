#ifndef __BUFFIO_MEMORY_HPP__
#define __BUFFIO_MEMORY_HPP__

/*
 * Error codes range reserved for buffioqueue
 *  [2500 - 3500]
 *  [Note] Errorcodes are negative value in this range.
 *  2500 <= errorcode <= 3500
 *
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <new>
#include <random>

// typiclly used by the user to get resource allocated
template <typename T> class buffioMemoryPool {

  // can give benefit as the data and next is decoupled form each other and can
  // also cause fragmentation as the struct is small
  struct buffioMemoryFragment {
    uintptr_t chksum; // can also be used as next ptr;
    T data;
  };

public:
  buffioMemoryPool() : fragments(nullptr) {};
  int init(size_t memReserve) {
    std::random_device rDev;
    std::mt19937::result_type seed =
        rDev() ^ (std::mt19937::result_type)
                     std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    std::mt19937 gen(seed);
    std::uniform_int_distribution<size_t> randomNumber(0, UINT64_MAX);
    chkSum = randomNumber(gen); // creating the chkSum for data integrity;
    reserveCount = memReserve != 0 ? memReserve : 10;
    fragments = nullptr;
    reserveMemory(reserveCount);

    return 0; // TODO: error check
  }

  T *getMemory() {
    if (fragments == nullptr) {
      if (reserveMemory(reserveCount) != 0)
        return nullptr;
    };
    buffioMemoryFragment *tmpFrag = fragments;
    fragments = (buffioMemoryFragment *)fragments->chksum;
    tmpFrag->chksum = chkSum;
    return &tmpFrag->data;
  };

  void retMemory(T *data) {
    if (data == nullptr)
      return;
    if (fragments == nullptr)
      reserveMemory(reserveCount);

    uintptr_t *chkSumLocal = (uintptr_t *)((char *)data - sizeof(uintptr_t));
    if (*chkSumLocal == chkSum) {
      *chkSumLocal = (uintptr_t)nullptr;
      pushFrag((buffioMemoryFragment *)data);
    }
    return;
  };

  ~buffioMemoryPool() { return; }

private:
  inline int reserveMemory(size_t count) {

    buffioMemoryFragment *tmp = nullptr;
    tmp = new (std::nothrow) buffioMemoryFragment;
    if (tmp == nullptr)
      return -1;

    tmp->chksum = (uintptr_t)nullptr;
    fragments = tmp;

    for (int i = 1; i < count; i++) {
      tmp = new (std::nothrow) buffioMemoryFragment;
      if (tmp == nullptr && count != 1)
        return 0; // atleast we reserved some memory

      tmp->chksum = (uintptr_t)fragments;
      fragments = tmp;
    }
    return 0;
  }
  inline void pushFrag(buffioMemoryFragment *data) {
    if (fragments == nullptr) {
      fragments = data;
      return;
    }
    data->chksum = (uintptr_t)fragments;
    fragments = data;
  };
  buffioMemoryFragment *fragments;
  size_t reserveCount;
  uintptr_t chkSum;
};

class buffiopage {
  struct page {
    char *buffer;
    size_t len;
    struct page *next;
  };

public:
  buffiopage() {}
  ~buffiopage() {}

  buffiopage &operator=(const buffiopage &) = delete;
  buffiopage(const buffiopage &) = delete;
  void pagewrite(char *data, size_t len) {} // get page length
  char *getpage(size_t len) {}              // getpage lenght

private:
  struct page *pages;
  struct page *next;
  size_t pagecount;
  size_t pagesize;
};

#endif
