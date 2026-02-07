#include "buffio/lfcore.hpp"

namespace buffio {
namespace lfCore {

void catchup(struct queueconf *which, buffioint tail, buffioint head) {
  while (!which->tail.compare_exchange_weak(tail, head,
                                            std::memory_order_acq_rel)) {
    head = which->head.load(std::memory_order_acquire);
    tail = which->tail.load(std::memory_order_acquire);
    if (buffio_cmp(tail, >=, head))
      break;
  };
};
bool lfenqueue(struct queueconf *which, size_t _order, size_t index) {

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

size_t lfdequeue(struct queueconf *which, size_t _order) {
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
      lfCore::catchup(which, tail, head + 1);
      which->threshold.store(-1, std::memory_order_release);
      return BUFFIO_EMPTY;
    }

    if (which->threshold.fetch_add(-1, std::memory_order_acq_rel) <= 0)
      return BUFFIO_EMPTY;
  };
};

void initempty(struct queueconf *which, size_t _order) {

  size_t n = buffiopow(_order + 1);
  for (size_t i = 0; i < n; i++)
    which->data[i] = (buffiosint)-1;

  which->head = 0;
  which->tail = 0;
  which->threshold = -1;
};
void initfull(struct queueconf *which, size_t _order) {
  size_t i = 0, half = buffiopow(_order), size = half << 1;
  for (; i < half; i++)
    which->data[cache_remap2(i, _order, size)] =
        size + cache_remap(i, _order, half);
  for (; i < size; i++)
    which->data[cache_remap2(i, _order, size)] = (buffiosint)-1;

  which->head = 0;
  which->tail = half;
  which->threshold = buffio_threshold(half, size);
};
}; // namespace lfCore
}; // namespace buffio
