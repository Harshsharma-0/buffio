#ifndef BUFFIO_QUEUE
#define BUFFIO_QUEUE

#include <cassert>
#include <type_traits>

enum class buffioQueueNoMem : int { no = 1 };
class blockQueue {
public:
  blockQueue *next;
  blockQueue *prev;
  blockQueue *waiter;
  buffioPromiseHandle current;

  ~blockQueue() { assert(current == nullptr); }
};

template <typename C = blockQueue,
          typename V = buffioPromiseHandle,
          typename D = void>
class buffioQueue {
  using memoryQueue = buffioMemoryPool<C>;

public:
  buffioQueue() : head(nullptr), tail(nullptr), count(0), poppedEntry(false) {
    assert(memory.init() == 0);
  };
  ~buffioQueue() = default;

  template<typename Z = V>
  requires std::is_same_v<Z,buffioPromiseHandle> 
  int push(V which, C *waiter = nullptr) {
    blockQueue *frag = nullptr;
    if ((frag = memory.pop()) == nullptr)
      return -1;
    if constexpr (std::is_same_v<C, blockQueue> &&
                  std::is_same_v<V, buffioPromiseHandle>) {
      frag->current = which;
      frag->waiter = waiter;
    };
    frag->next = nullptr;
    frag->prev = nullptr;
    return push(frag);
  };


  int pop(C *frag) {
    assert(count != 0 && head != nullptr);
    count -= 1;

    if (frag == frag->next) {
      memory.push(head);
      head = tail = nullptr;
      return 0;
    };

    frag->prev->next = frag->next;
    frag->next->prev = frag->prev;

    if (head == frag) {
      poppedEntry = true;
      head = frag->next;
    };
    if (tail == frag)
      tail = frag->next;

    if constexpr (std::is_same_v<D, void>) {
       memory.push(frag);
     };
      return 0;
  };

  int push(C *frag) {
    assert(frag != nullptr);
    count += 1;

    if (head == nullptr) {
      head = frag;
      tail = frag;
      frag->next = frag;
      frag->prev = frag;
      return 0;
    }

    frag->prev = tail->prev;
    frag->next = tail;
    tail->prev->next = frag;
    tail->prev = frag;

    return 0;
  };

  void erase() {
    assert(count != 0 && head != nullptr);
    count -= 1;
    poppedEntry = true;
    if (head == head->next) {
      memory.push(head);
      head = tail = nullptr;
      return;
    };
    head->prev->next = head->next;
    head->next->prev = head->prev;
    auto tmpHead = head;

    if (tail == head)
      tail = head->next;
    head = head->next;
  };

  void pop() {
    assert(count != 0 && head != nullptr);
    count -= 1;
    poppedEntry = true;
    if (head == head->next) {
    if constexpr (std::is_same_v<D, void>) {    
      memory.push(head);
     };
      head = tail = nullptr;
      return;
    };
    head->prev->next = head->next;
    head->next->prev = head->prev;
    auto tmpHead = head;

    if (tail == head)
      tail = head->next;
    head = head->next;

    if constexpr (std::is_same_v<D, void>) {
       memory.push(tmpHead);
     };
  };

  C *get() const {
    assert(head != nullptr && count != 0);
    return head;
  };
  void mvNext() {
    assert(head != nullptr && count != 0);
    if (poppedEntry) {
      poppedEntry = false;
      return;
    }
    head = head->next;
  };

  bool empty() const { return (count == 0); };

private:
  C *head;
  C *tail;
  size_t count;
  bool poppedEntry;
  memoryQueue memory;
};

#endif
