#ifndef __BUFFIO_LF_QUEUE__
#define __BUFFIO_LF_QUEUE__

/*
* IMPLEMENTAION BASED ON:
*  - https://rusnikola.github.io/files/ringpaper-disc.pdf
* 
*/

#include <atomic>
#include <cstdint>

template<typename T>
class buffiolfqueue{
 
public:
  buffiolfqueue(): data(nullptr),freequeue(nullptr)
                  ,aquiredqueue(nullptr){}

private:
 T *data;
 T *freequeue;
 T *aquiredqueue;

 std::atomic<int> head;
 std::atomic<int> tail;
 std::atomic<>
};
#endif

