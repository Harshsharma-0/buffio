#pragma once

#include "buffio/common.hpp"

#define CONTAINER_STORAGE_SIZE 64

namespace buffio {
  using containerCallback = void (*)(void *);

typedef struct functionInfo{
  void (*run)(void *data);
  void *data;
}functionInfo;
typedef struct container {

  alignas(std::max_align_t) char storage[CONTAINER_STORAGE_SIZE];
  void (*run)(void *data);
  void (*destroy)(void *data);

} container;

class makeContainer {
public:
  static void routine(buffio::promise &_promise, buffio::container &tmp);
  static void function(containerCallback callback, void *data,buffio::container &tmp);
};

}; // namespace buffio
