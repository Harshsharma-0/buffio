#ifndef BUFFIO_QUEUE
#define BUFFIO_QUEUE
#include "buffio/core.hpp"
#include "buffio/memory.hpp"
#include <cassert>
#include <sys/types.h>

namespace buffio {
template <
    typename taskQueueT = buffio::taskExt, typename mainValT = buffio::vTask,
    typename taskQueueAllocatorT = buffio::memory::pool<buffio::taskExt, 64>>
class taskQueue {
public:
  BUFFIO_CLASS_PROTECT(taskQueue)
  taskQueue() { taskCount = 0; };

  bool push(mainValT entry) {
    auto val = allocator();

    if (val == nullptr){
      return false;
    }
 
    val->task = entry;
    val->next = nullptr;

    head = head == nullptr ? val : head;
    head->next = val;
    head = val;
    
    
    if (tail == nullptr)
      tail = head;

    taskCount += 1;
    return true;
  };

  mainValT pop(mainValT onEmpty) {
    if (taskCount <= 0)
      return onEmpty;

    auto tmp = tail;
    mainValT work = tmp->task;
    tail = tail->next;
    allocator[tmp];
    taskCount -= 1;

    assert((taskCount >= 0));

    return work;
  };

  bool empty() const { return (taskCount <= 0); }

private:
  ssize_t taskCount;
  taskQueueT *head = nullptr;
  taskQueueT *tail = nullptr;
  taskQueueAllocatorT allocator;
};

} // namespace buffio

#endif
