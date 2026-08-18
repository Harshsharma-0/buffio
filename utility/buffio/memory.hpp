#ifndef BUFFIO_UTILITY_MEMORY
#define BUFFIO_UTILITY_MEMORY

#include "buffio/macro.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#define BUFFIO_MEMORY_MINSIZE sizeof(uintptr_t)
namespace buffio {

namespace memory {
namespace utility {
template <typename mainT, size_t chunkSizeN> constexpr size_t getStorageSize() {
  if constexpr (sizeof(mainT) <= BUFFIO_MEMORY_MINSIZE)
    return (BUFFIO_MEMORY_MINSIZE * chunkSizeN);

  return (sizeof(mainT) * chunkSizeN);
};
}; // namespace utility

template <typename poolT, size_t chunkSize> class pool {
public:
  struct poolChunk {
    static void *operator new(std::size_t size) noexcept {
      if (size == 0)
        size = 1;

      void *allocated = NULL;
      if ((allocated = std::malloc(size)) != NULL)
        return allocated;

      return nullptr;
    };
    static void operator delete(void *ptr) noexcept {
      if (ptr != nullptr)
        std::free(ptr);
    };

    struct poolChunk *next;
    size_t count;
    alignas(std::max_align_t) char storage
        [buffio::memory::utility::getStorageSize<poolT, chunkSize>()];
  };

  BUFFIO_CLASS_PROTECT(pool)
  pool() {
    chunkCount = freeCount = 0;
    chunks = nullptr;
    freeChunks = nullptr;
  };
 ~pool(){
   if(chunks == nullptr) return;
   auto ptr = chunks;
   auto ptrTmp = chunks;

   while(ptr != nullptr){
     ptrTmp = ptr;
     ptr = ptr->next;
     delete ptrTmp;
   }
 }
  poolT *operator()() noexcept { return pullOptimal(); };
  void operator[](poolT *ptr) { pushToFreeChunk(ptr); };

private:
  using poolFreeType = uintptr_t *;

  poolT *pullOptimal() {

    if (freeCount > 0) {
      poolFreeType *next = reinterpret_cast<poolFreeType*>(freeChunks[0]);
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
      return pullOptimal();
    }
    return nullptr;
  }
  void pushToFreeChunk(poolT *ptr) {
    poolFreeType *tmp = reinterpret_cast<poolFreeType*>(ptr);
    *tmp = reinterpret_cast<poolFreeType>(freeChunks);
    freeChunks = tmp;
    freeCount += 1;
  };

  bool makeChunk() {
    struct poolChunk *_chunk = new struct poolChunk;
    if (_chunk == nullptr)
      return false;
    _chunk->count = 0;
    _chunk->next = chunks;
    chunks = _chunk;
    chunkCount += 1;
    return true;
  };

  ssize_t chunkCount;
  ssize_t freeCount;
  struct poolChunk *chunks;
  poolFreeType *freeChunks;
};
}; // namespace memory
}; // namespace buffio
#endif
