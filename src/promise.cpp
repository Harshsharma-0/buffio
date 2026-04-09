#include "buffio/common.hpp"
#include "buffio/promise.hpp"
using pstripped = buffio::promise::promise_type;

namespace buffio {
void promise::run(void *data){
  auto task = (buffio::promise*)data;
  task->handle.resume();
  auto status = task->paddr->status;
  if(status == buffioRoutineStatus::done){
      task->handle.destroy();
      buffio::fiber::queue->pop();
  }
};

void promise::destroy(void *data){
  ((buffio::promise*)data)->handle.destroy();
};
void promise::setStatus(void *data,buffioRoutineStatus status){
 ((buffio::promise*)data)->paddr->status = status;
};

buffioRoutineStatus promise::checkStatus(void *data){
  return ((buffio::promise*)data)->paddr->status;
};
buffioHeader *promise::getHeader(void *data){
  return nullptr;
};

buffioAwaiter pstripped::await_transform(buffio::promise _promise) {

  auto entry = buffio::fiber::queue->getEntry();
  buffio::makeContainer::makeFromRoutine(_promise,entry->task);
  entry->waiter = buffio::fiber::queue->get();
  buffio::fiber::queue->erase();
  buffio::fiber::queue->push(entry);

    return {.ready = false};
};

buffioAwaiter pstripped::await_transform(buffioRoutineStatus ustatus) const {
  return {.ready = true};
};

buffioAwaiter pstripped::await_transform(buffio::clockSpec::wait wait) {
  auto current = buffio::fiber::queue->get();
  buffio::fiber::timerClock->push(wait.ms, current);
  buffio::fiber::queue->erase();
  return {.ready = false};
};
buffioAwaiter pstripped::await_transform(buffioHeader *header) {
  if (header == nullptr)
    return {.ready = true};

  header->entry = buffio::fiber::queue->get();  
  buffio::fiber::queue->erase();

  return {.ready = false};
};

buffioAwaiter pstripped::await_transform(fiber::clampInfo info) {
  if (info.header == nullptr)
    return {.ready = true};

  return {.ready = false};
};


}; // namespace buffio
