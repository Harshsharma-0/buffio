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
#include <optional>
#include <concepts>
#include <utility>
#include <memory>

namespace buffio {
enum lfMemMode {
  dynamic,
  stack,
};

namespace lfSpec {

template <size_t _order> constexpr size_t getSize() {
  if constexpr (_order > BUFFIO_RING_MAX || _order < BUFFIO_RING_MIN) {
    static_assert(false, BUFFIO_ARGS_STRINGFY(
                             order must be within range of[BUFFIO_RING_MIN] <=
                             order <= [BUFFIO_RING_MAX]));
    return 1; // just to supress other errors
  };
  return (1 << _order);
};

inline size_t get_size(size_t _order) {
  if (_order > BUFFIO_RING_MAX || _order < BUFFIO_RING_MIN) {
      return 0; // just to supress other errors
  };
  return (1 << _order);
};

template <size_t _rorder> constexpr size_t getInQueSize() {
  constexpr size_t size = buffio::lfSpec::getSize<_rorder>();
  return (size << 1);
};
}; // namespace lfSpec


template <typename T, buffio::lfMemMode lfmode = buffio::lfMemMode::dynamic,
          size_t orderT = 4>
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
    queueOrder = orderT;
    acQueue.data = aqQueue;
    freeQueue.data = fqQueue;
    lfCore::initempty(&acQueue, queueOrder);
    lfCore::initfull(&freeQueue, queueOrder);
  };

  int lfstart(size_t _order) requires(lfmode == buffio::lfMemMode::stack){
    constexpr size_t queueSize = buffio::lfSpec::getSize<orderT>();
     if(elock.create(queueSize) < 0) return -1;
     if(dlock.create(queueSize) < 0){
      elock.destroy();
      return -1;
    }
    return 0;
  };

  int lfstart(size_t _order)
    requires(lfmode == buffio::lfMemMode::dynamic)
  {


    if (data != nullptr) return 1;
    if(_order > buffioatomix_max_order || _order < BUFFIO_RING_MIN) return -1;
    

    size_t queueSize = 1 << _order;
    buffioatomix *acptr = nullptr;

    if(elock.create(queueSize) < 0) return -1;
    if(dlock.create(queueSize) < 0){
      elock.destroy();
      return -1;
    }

    if ((data = new (std::nothrow) T[queueSize]) == nullptr)
      return -1;

    if ((acptr = new (std::nothrow) buffioatomix[queueSize << 2]) == nullptr) {
      delete[] static_cast<T *>(data);
      data = nullptr;
      return -1;
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
    elock.wait();

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

    elock.post();
    return true;
  };

  bool empty() {
    return acQueue.threshold.load(std::memory_order_acquire) < 0 ? true : false;
  }

  std::optional<T> dequeue() {
    dlock.wait();
    size_t idx = lfCore::lfdequeue(&acQueue, queueOrder);
    T tmp;

    if (idx == BUFFIO_EMPTY){
      dlock.post();
      return std::nullopt;
    };

    if constexpr (lfmode == buffio::lfMemMode::dynamic) {
      tmp = static_cast<T*>(data)[idx];
    }
    if constexpr (lfmode == buffio::lfMemMode::stack) {
      tmp = data[idx];
    }
    lfCore::lfenqueue(&freeQueue, queueOrder, idx);

    dlock.post();
    return tmp;
  }

private:
  struct empty {};

  using dataType = std::conditional_t<
      lfmode == buffio::lfMemMode::stack, T[buffio::lfSpec::getSize<orderT>()],
      std::conditional_t<lfmode == buffio::lfMemMode::dynamic, void *, void>>;
  using qStorage =
      std::conditional_t<lfmode == buffio::lfMemMode::stack,
                         buffioatomix[buffio::lfSpec::getInQueSize<orderT>()],
                         struct empty>;

  dataType data;
  qStorage fqQueue;
  qStorage aqQueue;
  size_t queueOrder;
  buffio::semaphore elock;
  buffio::semaphore dlock;
  struct queueconf acQueue;
  struct queueconf freeQueue;
};

}; // namespace buffio
#endif
