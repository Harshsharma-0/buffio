#ifndef BUFFIO_TASK_CONTAINER
#define BUFFIO_TASK_CONTAINER
#include "buffio/core.hpp"
#include "buffio/macro.hpp"
#include "buffio/memory.hpp"
#include "buffio/queue.hpp"

namespace buffio {


typedef struct taskSelfDestruct {
  bool await_ready() noexcept { return ready; };
  void await_suspend(buffio::vTask) noexcept {}
  void await_resume() noexcept {}
  bool ready;
} taskSelfDestruct;


template <typename taskT> struct task : std::coroutine_handle<promise<taskT>> {
  using promise_type = buffio::promise<taskT>;
  bool schedule(buffio::instance &_instance) {
    instance = &_instance;
    this->promise().storage.instance = instance;
    return _instance.push({this->from_address(this->address())});
  }

  bool await_ready() { return false; }
  bool await_suspend(buffio::vTask waitingTask) {
    auto ptr = &this->promise().storage;
    ptr->waiter = waitingTask;
    ptr->waiterAvailable = true;
    instance = task<char>::from_address(waitingTask.address())
                   .promise()
                   .storage.instance;
    ptr->instance = instance;
    //if scheduling failed, immeadiately return control to the caller
    return instance->push(this->from_address(this->address()));
  };

  taskT await_resume() noexcept {
    taskT rval = this->promise().storage.val;
    this->destroy();
    return rval;
  };
  buffio::instance *instance;
};

template <typename promiseT> class promise : private buffio::core::promise {
public:
  task<promiseT> get_return_object() {
    storage.waiterAvailable = false;
    return {task<promiseT>::from_promise(*this)};
  };

  std::suspend_always initial_suspend() noexcept { return {}; };
  taskSelfDestruct final_suspend() noexcept {
    if (storage.waiterAvailable)
      storage.instance->push(storage.waiter);
    return {!storage.waiterAvailable};
  };

  template <std::convertible_to<promiseT> yFrom>
  std::suspend_always yield_value(yFrom &&from) {
    storage.val = std::forward<yFrom>(from);
    return {};
  }

  template <std::convertible_to<promiseT> rFrom>
  void return_value(rFrom &&from) {
    storage.val = std::forward<rFrom>(from);
  };
  void unhandled_exception() {};
  promisePacked<promiseT> storage;
};

using taskQueueDef = buffio::taskQueue<>;

}; // namespace buffio

#endif
