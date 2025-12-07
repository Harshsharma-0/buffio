#ifndef __BUFFIO_LF_QUEUE__
#define __BUFFIO_LF_QUEUE__

/*
* IMPLEMENTAION BASED ON:
*  - https://rusnikola.github.io/files/ringpaper-disc.pdf
*  - 1/12/25 read the research paper 
*/

#include <atomic>
#include <cstdint>

template<typename T>
class buffiolfqueue{

  struct __attribute__((packed)) queuebound{
   int32_t cycle :32;
   int32_t index :32;
  };

  struct queueconf{
    std::atomic<int> head;
    std::atomic<int> tail;
    std::atomic<int> threshold;
    queuebound *queue;
  };

public:
  buffiolfqueue(uint32_t rsize): data(nullptr),queuesize(0){
    if(rsize < 0) return;
    queuesize = (rsize >> 1) * 2;
    data = new T[queuesize]; 
  }
  ~buffiolfqueue(){ if(data != nullptr) delete data;}
 /*
   bool enqueue(T data){
   };

   T dequeue(){

   };
 */

private:


 T *data;
 uint32_t queuesize;
 std::atomic<int> nenqueuer; // number of concurrent enqueuer;
 std::atomic<int> ndequeuer; // number of concurrent dequeuer;
 std::atomic<int> safeaccess; // if -1 thread will wait, if 0 thread will continue;
 struct queueconf freequeue;
 struct queueconf acqueue;
};
#endif

