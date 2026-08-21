#ifndef BUFFIO_UTILITY_MEMORY
#define BUFFIO_UTILITY_MEMORY

#include "buffio/config.hpp"
#include "buffio/macro.hpp"
#include <utility>
#include <optional>
#include <new>

#define BUFFIO_MEMORY_MINSIZE sizeof(uintptr_t)

namespace buffio {

namespace utility{
inline std::pair<unsigned int,int> get_pow2(unsigned int size){
  
  /* check if the number is power of 2 or not it, then returns the order */
  if(size > 0 && (size & (size - 1)) == 0)
     return {size,(8 * sizeof(size) - __builtin_clz(size - 1))};
  
  int depth = ((int) size - 1);
  if(!depth)
      return {1U << 0 , -1};
  
  depth = 8 * sizeof(size) - __builtin_clz(size);

  return {1U << depth , depth};
}; 
};


namespace memory {
namespace utility {
template <typename mainT, size_t chunkSizeN> constexpr size_t GetStorageSize() {
  if constexpr (sizeof(mainT) <= BUFFIO_MEMORY_MINSIZE)
    return (BUFFIO_MEMORY_MINSIZE * chunkSizeN);

  return (sizeof(mainT) * chunkSizeN);
};
}; // namespace utility

template <typename poolT, size_t chunkSize> class pool {
public:
  struct poolChunk {
    struct poolChunk *next;
    size_t count;
    alignas(std::max_align_t) char storage
        [memory::utility::GetStorageSize<poolT, chunkSize>()];
  };

  BUFFIO_CLASS_PROTECT(pool)
  pool() {};
  
 ~pool(){

   if(chunks == nullptr) return;
   struct poolChunk *ptr = chunks;
   struct poolChunk *ptrTmp= nullptr;

   do{
     
     ptrTmp = ptr;
     ptr = ptr->next;
     delete ptrTmp;

   }while(ptr != nullptr);
  
 }
  poolT *operator()() noexcept { return pull_optimal(); };
  void operator[](poolT *ptr) { pushToFreeChunk(ptr); };

private:

  poolT *pull_optimal() {

    if (freeCount > 0) {
      uintptr_t *next = reinterpret_cast<uintptr_t*>(freeChunks[0]);
      poolT *data = reinterpret_cast<poolT *>(freeChunks);
      freeChunks = next;
      freeCount -= 1;
      return data;
    }

    if (chunkCount > 0) {
      size_t tCount = chunks->count;
      poolT *data = reinterpret_cast<poolT *>(chunks->storage);
      if (tCount < chunkSize) {
        chunks->count = tCount + 1;
        return (data + tCount);
      };
    };

    if (makeChunk()) {
      return pull_optimal();
    }
    return nullptr;
  }
  void pushToFreeChunk(poolT *ptr) {
    uintptr_t *tmp = reinterpret_cast<uintptr_t*>(ptr);
    *tmp = reinterpret_cast<uintptr_t>(freeChunks);
    freeChunks = tmp;
    freeCount += 1;
  };

  bool makeChunk() {
    struct poolChunk *_chunk = new(std::nothrow) struct poolChunk;
    if (_chunk == nullptr)
      return false;
    _chunk->count = 0;
    _chunk->next = chunks;
    chunks = _chunk;
    chunkCount += 1;
    return true;
  };

  ssize_t chunkCount = 0;
  ssize_t freeCount = 0;
  struct poolChunk *chunks = nullptr;
  uintptr_t *freeChunks = nullptr;
};
}; // namespace memory
}; // namespace buffio
#endif
