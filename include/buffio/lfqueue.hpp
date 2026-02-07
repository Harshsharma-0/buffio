#ifndef BUFFIO_LF_QUEUE
#define BUFFIO_LF_QUEUE

/*
 * IMPLEMENTAION BASED ON:
 *  - https://rusnikola.github.io/files/ringpaper-disc.pdf
 *  - github-repo: https://github.com/rusnikola/lfqueue
 */

#include "lfcore.hpp"

namespace buffio {
template <typename T> class lfqueue {

public:
  lfqueue() : data(nullptr) {
    acqueue.data = nullptr;
    freequeue.data = nullptr;
  };

  int lfstart(size_t _order) {
    if (_order <= buffioatomix_max_order && _order >= BUFFIO_RING_MIN &&
        data == nullptr) {
      queueorder = _order;
      try {
        data = new T[(1 << _order)];
      } catch (std::exception &e) {
        return -1;
      };

      try {
        acqueue.data = new buffioatomix[(1 << (_order + 1))];
      } catch (std::exception &e) {
        delete[] data;
        data = nullptr;
        return -1;
      }
      try {
        freequeue.data = new buffioatomix[(1 << (_order + 1))];
      } catch (std::exception &e) {
        delete[] data;
        delete[] acqueue.data;
        acqueue.data = nullptr;
        data = nullptr;
        return -1;
      };

      lfCore::initempty(&acqueue, _order);
      lfCore::initfull(&freequeue, _order);
      return 0;
    };
    return -1;
  }

  ~lfqueue() {
    if (data != nullptr)
      delete data;
    if (acqueue.data != nullptr)
      delete acqueue.data;
    if (freequeue.data != nullptr)
      delete freequeue.data;
    data = nullptr;
    acqueue.data = nullptr;
    freequeue.data = nullptr;
  }

  //  use enqueue and dequeue together don't mix locked enqueue and dequeue
  //  togethter
  bool enqueue(T data_) {
    size_t idx = lfCore::lfdequeue(&freequeue, queueorder);
    if (idx == BUFFIO_EMPTY)
      return false;
    data[idx] = data_;
    lfCore::lfenqueue(&acqueue, queueorder, idx);
    return true;
  };
  bool empty() {
    return acqueue.threshold.load(std::memory_order_acquire) < 0 ? true : false;
  }

  T dequeue(T onEmpty) {
    size_t idx = lfCore::lfdequeue(&acqueue, queueorder);
    if (idx == BUFFIO_EMPTY)
      return onEmpty;
    T tmp = data[idx];
    lfCore::lfenqueue(&freequeue, queueorder, idx);
    return tmp;
  }

private:
  T *data;
  size_t queueorder;
  struct queueconf acqueue;
  struct queueconf freequeue;
};
}; // namespace buffio
#endif
