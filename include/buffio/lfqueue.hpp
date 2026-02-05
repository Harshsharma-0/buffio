#ifndef BUFFIO_LF_QUEUE
#define BUFFIO_LF_QUEUE

/*
 * IMPLEMENTAION BASED ON:
 *  - https://rusnikola.github.io/files/ringpaper-disc.pdf
 *  - github-repo: https://github.com/rusnikola/lfqueue
 */

#include <atomic>
#include <cstdint>

#if defined(__arm__) || defined(__i386__) || defined(__powerpc__)
#define buffioatomix std::atomic<uint32_t>
#define buffiosatomix std::atomic<int32_t>
#define buffioint uint32_t
#define buffiosint int32_t

#define buffioatomix_max_order 25
#define BUFFIO_CACHE_FACTOR 7U
#define BUFFIO_CACHE_BYTES 1 << BUFFIO_CACHE_FACTOR
#define BUFFIO_RING_MIN (BUFFIO_CACHE_FACTOR - 3)
#define BUFFIO_LFQUEUE_MIN BUFFIO_RING_MIN
#define BUFFIO_LFQUEUE_MAX buffioatomix_max_order

#endif
#if defined(__aarch64__) || defined(__x86_64__) || defined(__powerpc64__)
#define buffioatomix std::atomic<uint64_t>
#define buffiosatomix std::atomic<int64_t>
#define buffioint uint64_t
#define buffiosint int64_t
#define buffioatomix_max_order 55

#define BUFFIO_CACHE_FACTOR 7U
#define BUFFIO_CACHE_BYTES 1 << BUFFIO_CACHE_FACTOR
#define BUFFIO_RING_MIN (BUFFIO_CACHE_FACTOR - 4)
#define BUFFIO_LFQUEUE_MIN BUFFIO_RING_MIN
#define BUFFIO_LFQUEUE_MAX buffioatomix_max_order

#endif

#define cache_remap(index, order, n)                                           \
  (size_t)(((index & (n - 1)) >> (order - BUFFIO_RING_MIN)) |                  \
           ((index << BUFFIO_RING_MIN) & n - 1))

#define cache_remap2(index, order, n) cache_remap(index, order + 1, n)
#define buffio_cmp(a, op, b) ((buffiosint)((a) - (b)) op 0)
#define buffio_threshold(half, size) ((long)((half) + (size) - 1)) //(3n -1)
#define buffiopow(order) (size_t)(1U << (order))
#define BUFFIO_EMPTY (~(size_t)0U) // == size_t_max;

template <typename T> class buffiolfqueue {

  struct queueconf {
    __attribute__((aligned(BUFFIO_CACHE_BYTES))) buffioatomix head;
    __attribute__((aligned(BUFFIO_CACHE_BYTES))) buffioatomix tail;
    __attribute__((aligned(BUFFIO_CACHE_BYTES))) buffiosatomix threshold;
    __attribute__((aligned(BUFFIO_CACHE_BYTES))) buffioatomix *data;
  };

public:
  buffiolfqueue() : data(nullptr) {
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

      initempty(&acqueue, _order);
      initfull(&freequeue, _order);
      return 0;
    };
    return -1;
  }

  ~buffiolfqueue() {
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
    size_t idx = lfdequeue(&freequeue, queueorder);
    if (idx == BUFFIO_EMPTY)
      return false;
    data[idx] = data_;
    lfenqueue(&acqueue, queueorder, idx);
    return true;
  };
  bool queuefull() {
    return acqueue.threshold.load(std::memory_order_acquire) < 0 ? true : false;
  }

  T dequeue(T onEmpty) {
    size_t idx = lfdequeue(&acqueue, queueorder);
    if (idx == BUFFIO_EMPTY)
      return onEmpty;
    T tmp = data[idx];
    lfenqueue(&freequeue, queueorder, idx);
    return tmp;
  }

private:
  static inline void catchup(struct queueconf *which, buffioint tail,
                             buffioint head) {
    while (!which->tail.compare_exchange_weak(tail, head,
                                              std::memory_order_acq_rel)) {
      head = which->head.load(std::memory_order_acquire);
      tail = which->tail.load(std::memory_order_acquire);
      if (buffio_cmp(tail, >=, head))
        break;
    };
  };

  static inline bool lfenqueue(struct queueconf *which, size_t _order,
                               size_t index) {
    size_t half = buffiopow(_order), size = (half << 1), tidx = 0;
    buffioint tail = 0, entry = 0, entcycle = 0, tailcycle = 0;

    index ^= (size - 1); // encoding index and (size - 1) together to later mask
                         // cycle and tidx together.

    while (1) {
      tail = which->tail.fetch_add(
          1, std::memory_order_acq_rel); // fetching and adding 1 to tail.
      tailcycle =
          (tail << 1) |
          ((size << 1) -
           1); // extracting the cycle of the tail the "high-bits". here we mask
               // out index bit's to '1', leaving only cycle bits valid
      tidx = cache_remap2(tail, _order,
                          size); // remapping the tail index to the index we
                                 // want, distributing across cachelines.
      entry = which->data[tidx].load(
          std::memory_order_acquire); // loading the entry form that index.
      entcycle =
          entry | ((size << 1) -
                   1); // extracting the cycle of entry. here we mask out the
                       // index bit's to '1', leaving only cycle bits valid
      // entry == ecycle when the entry is free and there is no index in there.

    retry:
      if (buffio_cmp(entcycle, <, tailcycle) && ((entry == entcycle)) ||
          ((entry == (entcycle ^ size)) &&
           buffio_cmp(which->head.load(std::memory_order_acquire), <=, tail))) {
        if (!which->data[tidx].compare_exchange_weak(entry, (tailcycle ^ index),
                                                     std::memory_order_acq_rel))
          goto retry;

        if (which->threshold.load(std::memory_order_acquire) !=
            buffio_threshold(half, size))
          which->threshold.store(buffio_threshold(half, size),
                                 std::memory_order_release);

        return true;
      };
    };
  };

  static inline size_t lfdequeue(struct queueconf *which, size_t _order) {
    if (which->threshold.load(std::memory_order_acquire) < 0)
      return BUFFIO_EMPTY;
    size_t half = buffiopow(_order), size = (half << 1), tidx = 0, attempt = 0;
    buffioint head = 0, entry = 0, entcycle = 0, headcycle = 0,
              mask = (size << 1) - 1, entnew = 0, tail = 0;

    while (1) {
      head = which->head.fetch_add(1, std::memory_order_acq_rel);
      tidx = cache_remap2(head, _order, size);
      headcycle = (head << 1) | mask;

    again:
      entry = which->data[tidx].load(std::memory_order_acquire);

      do {
        entcycle = entry | mask;
        if (entcycle == headcycle) {
          which->data[tidx].fetch_or((size - 1), std::memory_order_acq_rel);
          return (size_t)(entry & (size - 1));
        }
        if ((entry | size) != entcycle) {
          entnew = entry & ~(buffioint)size;
          if (entry == entnew)
            break;
        } else {
          if (++attempt <= 5000)
            goto again;
          entnew = headcycle ^ ((~entry) & size);
        };
      } while (buffio_cmp(entcycle, <, headcycle) &&
               !which->data[tidx].compare_exchange_weak(
                   entry, entnew, std::memory_order_acq_rel));

      tail = which->tail.load(std::memory_order_acquire);
      if (tail <= (head + 1)) {
        catchup(which, tail, head + 1);
        which->threshold.store(-1, std::memory_order_release);
        return BUFFIO_EMPTY;
      }

      if (which->threshold.fetch_add(-1, std::memory_order_acq_rel) <= 0)
        return BUFFIO_EMPTY;
    };
  };

  static inline void initempty(struct queueconf *which, size_t _order) {
    size_t n = buffiopow(_order + 1);
    for (size_t i = 0; i < n; i++)
      which->data[i] = (buffiosint)-1;

    which->head = 0;
    which->tail = 0;
    which->threshold = -1;
  };
  static inline void initfull(struct queueconf *which, size_t _order) {
    size_t i = 0, half = buffiopow(_order), size = half << 1;
    for (; i < half; i++)
      which->data[cache_remap2(i, _order, size)] =
          size + cache_remap(i, _order, half);
    for (; i < size; i++)
      which->data[cache_remap2(i, _order, size)] = (buffiosint)-1;

    which->head = 0;
    which->tail = half;
    which->threshold = buffio_threshold(half, size);
  }

  T *data;
  size_t queueorder;
  struct queueconf acqueue;
  struct queueconf freequeue;
};
#endif
