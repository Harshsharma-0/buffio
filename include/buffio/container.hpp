#pragma once

#include "buffio/common.hpp"

#define CONTAINER_STORAGE_SIZE 64

namespace buffio {
typedef struct container {

  alignas(std::max_align_t) char storage[CONTAINER_STORAGE_SIZE];
  void (*run)(void *data);
  void (*destroy)(void *data);
  buffioRoutineStatus (*getStatus)(void *data);
  void (*setStatus)(void *data, buffioRoutineStatus status);
  buffioHeader *(*getHeader)(void *data);

} container;

class makeContainer {
public:
  static void makeFromRoutine(buffio::promise &_promise, buffio::container &tmp);
};

}; // namespace buffio
