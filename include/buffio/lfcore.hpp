#pragma once

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

struct queueconf {
  __attribute__((aligned(BUFFIO_CACHE_BYTES))) buffioatomix head;
  __attribute__((aligned(BUFFIO_CACHE_BYTES))) buffioatomix tail;
  __attribute__((aligned(BUFFIO_CACHE_BYTES))) buffiosatomix threshold;
  __attribute__((aligned(BUFFIO_CACHE_BYTES))) buffioatomix *data;
};
namespace buffio {
namespace lfCore {

void catchup(struct queueconf *which, buffioint tail, buffioint head);
bool lfenqueue(struct queueconf *which, size_t _order, size_t index);
size_t lfdequeue(struct queueconf *which, size_t _order);
void initempty(struct queueconf *which, size_t _order);
void initfull(struct queueconf *which, size_t _order);

}; // namespace lfCore
}; // namespace buffio
