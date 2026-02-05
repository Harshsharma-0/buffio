#ifndef __BUFFIO_MEMORY_HPP__
#define __BUFFIO_MEMORY_HPP__

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <random>

/**
 * @file buffiomemory.hpp
 * @author Harsh Sharma
 * @brief Core Memory allocator and deallocator of buffio
 *
 * buffioMemoryPool is a memory manager and allocator for a specific,
 * data types, and provide memory for that type when ever needed.
 *
 */

/**
 * @class buffioMemoryPool
 * @brief Core Memory allocator for buffio memory needs.
 *
 * @details
 * buffioMemoryPool:
 * - Owns every memory allocated within it self
 * - CheckSum based verification to verify the memory given back
 * - Allocated memory in pages, to reduce fragmentation, and each page have
 * memory fragments that is given out
 *
 * checksum is used to check, if the memory has maintained, it integrity after use,
 * and is usefull to find memorybugs for buffers, as if one buffer overflows, it's invalidates
 * the checksum of other memory region, corrupting other memory regions.
 *
 * Typical Usage:
 * 1. Create buffioMemoryPool instance, with the memory type in the template arg
 * 2. use get() to get memory and push() to give memory back.
 * 3. If the instance of buffioMemoryPool goes out of scope of the function,
 * that owns it, all the memory allocated is freed.
 */

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
  /**
   * @brief Construct a new instance, with default initlization of parameters
   *
   */
  buffioMemoryPool()
      : fragments(nullptr), pageFragmentCount(0), pageHead(nullptr),
        customDeleter(nullptr), inUse(nullptr) {};
  /**
   * @brief Destroys the instance, if the instance goes out of scope
   *
   */
  ~buffioMemoryPool() { release(); };

  /**
   * @brief method used to release all the pages and deallocate memory allocated by the pool.
   *
   * @returns void
   */
  void release() {
    buffioMemoryPages *tmpPage = nullptr;
    while (pageHead != nullptr) {
      delete[] pageHead->data;
      tmpPage = pageHead;
      pageHead = pageHead->next;
      delete tmpPage;
    };
  };

  /**
   * @brief static method used for check sum generation
   *
   * @return uintptr_t - generated number;
   */
  static uintptr_t genChkSum() {
    std::random_device rDev;
    std::mt19937::result_type seed =
        rDev() ^ (std::mt19937::result_type)
                     std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    std::mt19937 gen(seed);
    std::uniform_int_distribution<size_t> randomNumber(0, UINT64_MAX);
    return static_cast<uintptr_t>(randomNumber(gen));
  };
  /**
   * @brief method used to initlise the pool with pages
   *
   * init must me called before using memorypoll, as to initlise the page,
   * with the pagesize.
   *
   * @param[in] fragmentCount number of memory chunk per page
   * @param[in] void(*deleter)(T *data) - custom deleter to call for all the fagments in the page.
   *
   * @return 0 on success , -1 on error. Any value below 0 is treated as error.
   *
   */
  int init(size_t fragmentCount = 25, void (*deleter)(T *data) = nullptr) {

    chkSum = buffioMemoryPool::genChkSum(); // creating the chkSum for data
// integrity;
    pageFragmentCount = fragmentCount;
    fragments = nullptr;
    return makePage(); // TODO: error check
  }

  /**
   * @brief method to get memory from the pool
   * 
   * pop() method is used to get the memory from the pool.
   * - pop workings:
   *   1. if there memory available, it immediately return with the memory
   *   2. if there is no memory availabe, a new page is allocated and memory is returned.
   *
   * @return pointer to the memoryfragment, or nullptr if there any error occured.
   *
   */

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

/**
 * @brief method to push back the memory after use
 *
 * push() method accepts only the memory that was obtained from the memory pool,
 * any other memory from other sources(user allocated memory), causes the assertion to
 * fail. 
 * 
 *
 * @param[in] data memory fragment to push back.
 *
 */

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
