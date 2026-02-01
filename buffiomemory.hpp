#ifndef __BUFFIO_MEMORY_HPP__
#define __BUFFIO_MEMORY_HPP__


#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <random>

/*
 * ===============================================================================
 *
 *
 *  buffioPage
 *
 * ===============================================================================
 */

// typiclly used by the user to get resource allocated
template <typename T> class buffioMemoryPool {

  // can give benefit as the data and next is decoupled form each other and can
  // also cause fragmentation as the struct is small
  struct buffioMemoryFragment {
    uintptr_t chksum; // can also be used as next ptr;
    T data;
    ~buffioMemoryFragment() = default;
  };
  struct buffioMemoryPages {
    struct buffioMemoryPages *next;
    struct buffioMemoryFragment *data;
  };

public:
  buffioMemoryPool()
      : fragments(nullptr), pageFragmentCount(0), pageHead(nullptr),
        customDeleter(nullptr), inUse(nullptr) {};
  ~buffioMemoryPool() { release(); };

  void release() {
    buffioMemoryPages *tmpPage = nullptr;
    while (pageHead != nullptr) {
      delete[] pageHead->data;
      tmpPage = pageHead;
      pageHead = pageHead->next;
      delete tmpPage;
    };
  }
  int init(size_t fragmentCount = 250, void (*deleter)(T *data) = nullptr) {
    std::random_device rDev;
    std::mt19937::result_type seed =
        rDev() ^ (std::mt19937::result_type)
                     std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    std::mt19937 gen(seed);
    std::uniform_int_distribution<size_t> randomNumber(0, UINT64_MAX);
    chkSum = randomNumber(gen); // creating the chkSum for data integrity;
    pageFragmentCount = fragmentCount;
    fragments = nullptr;
    return makePage(); // TODO: error check
  }

  T *getMemory() {
    if (fragments == nullptr) {
      if (makePage() != 0)
        return nullptr;
    };
    buffioMemoryFragment *tmpFrag = fragments;
    fragments = (buffioMemoryFragment *)fragments->chksum;
    tmpFrag->chksum = chkSum;
    return &tmpFrag->data;
  };

  T *pop() {
    if (fragments == nullptr) {
      if (makePage() != 0)
        return nullptr;
    };
    buffioMemoryFragment *tmpFrag = fragments;
    fragments = (buffioMemoryFragment *)fragments->chksum;
    tmpFrag->chksum = chkSum;
    return &tmpFrag->data;
  };

  void push(T *data) {
    if (data == nullptr)
      return;
    if (fragments == nullptr)
      makePage();

    uintptr_t *chkSumLocal =
        (uintptr_t *)((uintptr_t)data -
                      offsetof(struct buffioMemoryFragment, data));
    assert(*chkSumLocal == chkSum);
    *chkSumLocal = (uintptr_t)nullptr;
    pushFragment((buffioMemoryFragment *)chkSumLocal);
    return;
  };

private:
  inline int makePage() {

    buffioMemoryPages *tmpPage = nullptr;
    try {
      tmpPage = new buffioMemoryPages;
    } catch (std::exception &e) {
      return -1;
    };

    buffioMemoryFragment *tmpFragment = nullptr;
    try {
      tmpFragment = new buffioMemoryFragment[pageFragmentCount];
    } catch (std::exception &e) {
      delete tmpPage;
      return -1;
    };

    tmpPage->data = tmpFragment;
    tmpPage->next = nullptr;
    makeFragementFromPage(tmpPage);

    if (pageHead == nullptr) {
      pageHead = tmpPage;
      return 0;
    }

    tmpPage->next = pageHead;
    pageHead = tmpPage;
    return 0;
  };

  inline void makeFragementFromPage(buffioMemoryPages *fromPage) {
    for (size_t i = 0; i < pageFragmentCount; i++) {
      pushFragment(&fromPage->data[i]);
    }
  };

  inline void pushFragment(buffioMemoryFragment *data) {

    data->chksum = (uintptr_t)nullptr;
    if (fragments == nullptr) {
      fragments = data;
      return;
    }
    data->chksum = (uintptr_t)fragments;
    fragments = data;
  };

  buffioMemoryPages *pageHead;
  buffioMemoryFragment *inUse;
  buffioMemoryFragment *fragments;
  void (*customDeleter)(T *data);
  size_t pageFragmentCount;
  uintptr_t chkSum;
};

/*
 * ===============================================================================
 *
 * buffioPage
 *
 * ===============================================================================
 */
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
  char *getpage(size_t len) { return 0; }   // getpage lenght

private:
  struct page *pages;
  struct page *next;
  size_t pagecount;
  size_t pagesize;
};

#endif
