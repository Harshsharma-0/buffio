#ifndef BUFFIO_QUEUE
#define BUFFIO_QUEUE
#include "buffio/config.hpp"
#include "buffio/core.hpp"
#include "buffio/memory.hpp"
#include "buffio/ecode.hpp"
#include <cassert>
#include <optional>

namespace buffio {

template <typename QueueT> class Queue {

  static constexpr unsigned int queue_internal_order = 6;
  static constexpr unsigned int queue_internal_max = (1 << 6);
  struct queue_internal {
    struct {
      struct queue_internal *next = nullptr;
      unsigned int head = 0;
      unsigned int tail = 0;
    } state;
    QueueT data[queue_internal_max];
  };

public:
  BUFFIO_CLASS_PROTECT(Queue)
  Queue() {};

  int init() {

    assert(sp_head == nullptr);

    sp_head = allocator();
    sp_tail = sp_head;

    return sp_head == nullptr ? B_ENOMEM : 0;
  }

  int enqueue(QueueT entry) {
    assert(sp_head != nullptr);

    QueueT *data = &sp_tail->data[0];
    auto [next, head, tail] = sp_tail->state;
    
    unsigned int size = queue_internal_max - 1;
    unsigned int cycle_tail = (tail | (size - 1));
    unsigned int cycle_head = (head | (size - 1));

    int cycle = static_cast<int>(cycle_tail) - static_cast<int>(cycle_head);

    head &= size;
    tail &= size;


    if (head == tail && cycle > 0) {

      struct queue_internal *tmp = nullptr;
      if ((tmp = allocator()) == nullptr) return B_ENOMEM;
      sp_tail->state.next = tmp;
      sp_tail = tmp;
      
      tmp->state.head = 0;
      tmp->state.tail = 0;
      tmp->state.next = nullptr;

      data = &sp_tail->data[0];
      cycle_tail = tail = 0;

    };
    auto *val = &entry;

    data[tail++] = entry;

    // incrementing the tail directly to preserve cycle
    sp_tail->state.tail += 1;
    count_ += 1;

    return true;
  };

  std::optional<QueueT> dequeue() {
    assert(sp_head != nullptr);

    auto [next, head, tail] = sp_head->state;
    QueueT *data = &sp_head->data[0];

    unsigned int size = queue_internal_max - 1;
    unsigned int cycle_tail = (tail | (size - 1));
    unsigned int cycle_head = (head | (size - 1));

    head &= size;
    tail &= size;
  
    int cycle = static_cast<int>(cycle_tail) - static_cast<int>(cycle_head);


    if (head == tail && cycle == 0) {
      if (next == nullptr)
         return std::nullopt;

      allocator[sp_head];
      sp_head = next;
      data = &sp_head->data[0];
      head = sp_head->state.head;

    };

    QueueT dtmp = data[head++];

    // incrementing the head directly to preserve cycle
    sp_head->state.head += 1;

    assert(count_ != 0);
    count_ -= 1;

    return dtmp;
  };

  bool empty() const { return (count_ <= 0); };
  size_t count() const { return count_; };

private:
  size_t count_ = 0;
  queue_internal *sp_tail = nullptr; // enqueue from tail entry
  queue_internal *sp_head = nullptr; // dequeue from head entry
  buffio::memory::pool<queue_internal, 1> allocator;
};

} // namespace buffio

#endif
