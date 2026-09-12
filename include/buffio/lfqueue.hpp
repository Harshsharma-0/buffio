#ifndef BUFFIO_LFQUEUE
#define BUFFIO_LFQUEUE

/*
 * IMPLEMENTAION BASED ON:
 *  - https://rusnikola.github.io/files/ringpaper-disc.pdf
 *  CODE COPYED FROM:
 *  - github-repo: https://github.com/rusnikola/lfqueue
 */

#include "buffio/lfcore.hpp"
#include "buffio/macro.hpp"
#include "buffio/thread.hpp"
#include <iostream>
#include <optional>
#include <concepts>
#include <utility>
#include <memory>

namespace buffio {
enum lfMemMode {
  dynamic,
  stack,
};

namespace lfutility {

template <size_t _order> constexpr size_t get_size() {
  if constexpr (_order > BUFFIO_RING_MAX || _order < BUFFIO_RING_MIN) {
    static_assert(false, BUFFIO_ARGS_STRINGFY(
                             order must be within range of[BUFFIO_RING_MIN] <=
                             order <= [BUFFIO_RING_MAX]));
    return 1; // just to supress other errors
  };
  return (1 << _order);
};

}; // namespace lfSpec


template <typename T, buffio::lfMemMode lfmode = buffio::lfMemMode::dynamic,
          size_t lforder =  8>
class lfQueue {
public:
  BUFFIO_CLASS_PROTECT(lfQueue)

  lfQueue()
    requires(lfmode == buffio::lfMemMode::dynamic)
  {
    data = nullptr;
    acQueue.data = nullptr;
    freeQueue.data = nullptr;
  };

  lfQueue()
    requires(lfmode == buffio::lfMemMode::stack)
  {

    
    /* only to validate the order */
    constexpr size_t validate_order = buffio::lfutility::get_size<lforder>();

    queueOrder = lforder;
    acQueue.data = aqQueue;
    freeQueue.data = fqQueue;
    lfCore::initempty(&acQueue, queueOrder);
    lfCore::initfull(&freeQueue, queueOrder);
  };

  int lfstart(size_t _order) requires(lfmode == buffio::lfMemMode::stack){ 
    return 0;
  };

  int lfstart(size_t _order)
    requires(lfmode == buffio::lfMemMode::dynamic)
  {


    if (data != nullptr) return B_EALREADY;
    if(_order > buffioatomix_max_order || _order < BUFFIO_RING_MIN)
      return B_ELFORDER;
    
    size_t queueSize = 1 << _order;
    buffioatomix *acptr = nullptr;


    if ((data = new (std::nothrow) T[queueSize]) == nullptr)
      return B_ENOMEM;

    if ((acptr = new (std::nothrow) buffioatomix[queueSize << 2]) == nullptr) {
      delete[] static_cast<T *>(data);
      data = nullptr;
      return B_ENOMEM;
    };

    acQueue.data = acptr;
    freeQueue.data = &acptr[queueSize << 1];
    queueOrder = _order;

    lfCore::initempty(&acQueue, _order);
    lfCore::initfull(&freeQueue, _order);
    return 0;
  }

  ~lfQueue()
    requires(lfmode == buffio::lfMemMode::stack)
  = default;

  ~lfQueue()
    requires(lfmode == buffio::lfMemMode::dynamic)
  {
    if (data != nullptr)
      delete[] static_cast<T *>(data);

    if (acQueue.data != nullptr)
      delete[] acQueue.data;

    data = nullptr;
    acQueue.data = nullptr;
    freeQueue.data = nullptr;
  }

  bool enqueue(T data_) {

    size_t idx = lfCore::lfdequeue(&freeQueue, queueOrder);
    if (idx == BUFFIO_EMPTY)
      return false;

    if constexpr (lfmode == buffio::lfMemMode::dynamic) {
      static_cast<T*>(data)[idx] = data_;
    }

    if constexpr (lfmode == buffio::lfMemMode::stack) {
      data[idx] = data_;
    }

    lfCore::lfenqueue(&acQueue, queueOrder, idx);
    
    entry_count.fetch_add(1,std::memory_order_release);
    return true;
  };

  bool empty() {
    return acQueue.threshold.load(std::memory_order_acquire) < 0 ? true : false;
  }

  std::optional<T> dequeue() {
    size_t idx = lfCore::lfdequeue(&acQueue, queueOrder);
    T tmp;

    if (idx == BUFFIO_EMPTY){
         return std::nullopt;
    };

    if constexpr (lfmode == buffio::lfMemMode::dynamic) {
      tmp = static_cast<T*>(data)[idx];
    }
    if constexpr (lfmode == buffio::lfMemMode::stack) {
      tmp = data[idx];
    }
    lfCore::lfenqueue(&freeQueue, queueOrder, idx);
    entry_count.fetch_add(-1,std::memory_order_release);
    return tmp;
  };
   
   size_t count() const { return entry_count.load(std::memory_order_acquire); };

private:
  struct empty {};

  using dataType = std::conditional_t<
      lfmode == buffio::lfMemMode::stack, 
      T[buffio::lfutility::get_size<lforder>()],
      std::conditional_t<lfmode == buffio::lfMemMode::dynamic, void *, void>>;

  using qStorage =
      std::conditional_t<lfmode == buffio::lfMemMode::stack,
                         buffioatomix[buffio::lfutility::get_size<lforder>()],
                         struct empty>;

  dataType data;
  qStorage fqQueue;
  qStorage aqQueue;
  buffioatomix entry_count;
  size_t queueOrder;
  struct queueconf acQueue;
  struct queueconf freeQueue;
};

}; // namespace buffio
#endif
