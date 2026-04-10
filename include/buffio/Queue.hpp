#pragma once

#include "buffio/container.hpp"
#include "buffio/memory.hpp"
#include <cassert>
#include <iostream>
#include <type_traits>
/**
 * @file buffioQueue.hpp
 * @author Harsh Sharma
 * @brief Core Queue implementation of all the queue used in buffio.
 *
 *
 */

/**
*@brief template arguement used to disable internal memory allocation and
 management.
*/


enum class buffioQueueNoMem : int { no = 1 };

/**
 * @brief buffiomain queue structure defination.
 */
class blockQueue {
public:
  blockQueue *next;              ///< next member of the queue.
  buffio::container task;        ///< task handle of the task
  blockQueue *waiter;            ///< waiter for the task.
  blockQueue *prev;              ///< previous member of the queue.
};

/**
 * @class buffioQueue
 * @brief Central Queue Management implementation for buffio.
 *
 * @details
 * buffioQueue is a self-contained queue management class that:
 * - Owns every entry that is given out to prevent memoryleaks.
 * - Can be configured to be a linear queue, or circular queue.
 *
 *
 * Typical Mode Configuration:
 * 1. Ciruclar Queue - circular double-linked list mode.
 * 2. linear queue - linear linked-list mode.
 * 3. Watcher Queue - queue just to keep entry.
 * 4. External Memory Managed Queue - Memory of entry managed by externally.
 */

namespace buffio {

template <typename C = blockQueue, typename V = buffio::container,
          typename D = void>

class Queue {
  using memoryQueue = buffio::Memory<blockQueue>;

public:
  /**
   * @brief Default constructor of queue.
   *
   */
  Queue() : head(nullptr), count(0){
    assert(memory.init() == 0);
  };
  /**
   *@brief Default destructor of the queue.
   */
  ~Queue() = default;

  template <typename Z = V>
    requires std::is_same_v<Z, buffio::promiseHandle>
  int push(V which, C *waiter = nullptr) {
    blockQueue *frag = nullptr;
    if ((frag = memory.pop()) == nullptr)
      return -1;
    if constexpr (std::is_same_v<C, blockQueue> &&
                  std::is_same_v<V, buffio::container>) {
      frag->task = which;
      frag->waiter = waiter;
    };
    return push(frag);
  };
    C *getEntry() { return memory.pop();}
  /**
   * @brief method used to remove a entry from the queue, from any position.
   *
   * @param[in] frag The entry to remove from the queue.
   */
  int pop(C *frag) {
    assert(count != 0 && head != nullptr);
    count -= 1;

    if (frag == frag->next) {
      if constexpr (std::is_same_v<D, void>) {
        memory.push(frag);
      };

      head =  nullptr;
      return 0;
    };

    frag->prev->next = frag->next;
    frag->next->prev = frag->prev;

    if (head == frag) {
      head = frag->next;
    };

    if constexpr (std::is_same_v<D, void>) {
      memory.push(frag);
    };
    return 0;
  };

  /**
   * @brief method used to push a new/popped entry back the queue.
   * @param[in] frag entry that is going to be added in the queue.
   */
  int push(C *frag) {
    assert(frag != nullptr);
    count += 1;
    if (head == nullptr) {
      head = frag;
      frag->next = frag;
      frag->prev = frag;
      return 0;
    }

    frag->prev = head->prev;
    frag->next = head;
    head->prev->next = frag;
    head->prev = frag;

    return 0;
  };

  int pushHead(C *frag) {
    push(frag);
    head = frag;
    return 0;
  };
  /**
   * @brief method used to erase the entry that
   *
   *
   */
  void erase() {
    assert(count != 0 && head != nullptr);
    count -= 1;
    if (head == head->next) {
      head = nullptr;
      return;
    };

    head->prev->next = head->next;
    head->next->prev = head->prev;
    head = head->next;
  };

  void pop() {
    assert(count != 0 && head != nullptr);
    count -= 1;
    if (head == head->next) {
      if constexpr (std::is_same_v<D, void>) {
        memory.push(head);
      };
      head =  nullptr;
      return;
    };
    head->prev->next = head->next;
    head->next->prev = head->prev;
    
    if constexpr (std::is_same_v<D, void>) {
      auto tmpHead = head;
      memory.push(tmpHead);
    };
    head = head->next;
  };

  C *get() const {
    assert(head != nullptr && count != 0);
    return head;
  };
  void mvNext() {
    assert(head != nullptr && count != 0);
    head = head->next;
  };

  bool empty() const { return (count == 0); };
  size_t gcount() const { return count; }

private:
  C *head;
  size_t count;
  memoryQueue memory;
};

}; // namespace buffio
