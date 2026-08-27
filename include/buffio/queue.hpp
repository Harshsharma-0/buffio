#ifndef BUFFIO_QUEUE
#define BUFFIO_QUEUE
#include "buffio/config.hpp"
#include "buffio/core.hpp"
#include "buffio/memory.hpp"
#include <cstring>
#include <optional>
#include <cassert>
#include <iostream>

namespace buffio {

template <typename QueueT>
class Queue {
  
static constexpr unsigned int queue_internal_order = 6;
static constexpr unsigned int queue_internal_max = (1 << 6);
struct queue_internal{
    struct{
     struct queue_internal *next = nullptr;
     unsigned int head  = 0;
     unsigned int tail  = 0;
    }state;
    QueueT data[queue_internal_max];
};
  

public:
  BUFFIO_CLASS_PROTECT(Queue)
  Queue(){};
  
  
  bool init(){
   
   assert(sp_head == nullptr);

   sp_head = allocator();
   sp_tail = sp_head;
   return sp_head == nullptr ? false : true;

  }

  bool enqueue(QueueT entry) {
    assert(sp_head != nullptr);
   
    auto [next,head,tail] = sp_tail->state;
    QueueT *data = &sp_tail->data[0];

    unsigned int cycle_tail = (tail >> queue_internal_order);
    unsigned int cycle_head = (head >> queue_internal_order); 
    int cycle = 
        static_cast<int>(cycle_tail) - static_cast<int>(cycle_head);

    head ^= (cycle_head << queue_internal_order);
    tail ^= (cycle_tail << queue_internal_order);

    /* case : if difference positive then cycle not same.
     * case : if negative tail is wraps around in cycle with head;
     * case : if 0 queue is empty
     */
    
    /* code handle full queue */
     bool cycle_check = (cycle < 0 || cycle > 0);

      if(head == tail && cycle_check){


       struct queue_internal *tmp = nullptr;
       if((tmp = allocator()) == nullptr) return false;

       sp_tail->state.next = tmp;
       sp_tail = tmp;

       std::memset((void *)sp_tail,'\0',sizeof(struct queue_internal));

       data = &sp_tail->data[0];
       cycle_tail = tail = 0;
      };
   
    data[tail] = entry;
    sp_tail->state.tail = tail + 1;
    count_ += 1;
    return true;
  };

  std::optional<QueueT> dequeue() {

    assert(sp_head != nullptr); 

    auto [next,head,tail] = sp_head->state;
    QueueT *data = &sp_head->data[0];

    unsigned int cycle_tail = (tail >> queue_internal_order);
    unsigned int cycle_head = (head >> queue_internal_order); 
    int cycle = 
      static_cast<int>(cycle_tail) - static_cast<int>(cycle_head);

    head ^= (cycle_head << queue_internal_order);
    tail ^= (cycle_tail << queue_internal_order);

  /* case : if difference positive then cycle not same.
   * case : if negative tail is wraps around in cycle with head;
   * case : if 0 queue is empty
   */

   if(tail == head && cycle == 0){
     if(next == nullptr) return std::nullopt;
     allocator[sp_head];

     sp_head = next;
     data = &sp_head->data[0];
     head = sp_head->state.head;

   };

   QueueT dtmp = data[head];
   sp_head->state.head = head + 1;
  
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
