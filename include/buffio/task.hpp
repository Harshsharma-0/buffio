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


template <typename taskT> struct task :
  std::coroutine_handle<promise<taskT>>,
  core::task
 {
  using promise_type = buffio::promise<taskT>;

  bool schedule(buffio::instance &_instance) {
    instance = &_instance;
    this->promise().storage.instance = &_instance;
    return core_schedule(_instance,{this->from_address(this->address())});
  }

  bool await_ready() { return false; }
  bool await_suspend(buffio::vTask waiting_task) {

    //if scheduling failed, immeadiately return control to the caller
    return core_promise_and_push(this->promise().storage,
          waiting_task,
          {this->from_address(this->address()).promise()});

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
    return_val = std::forward<yFrom>(from);
    return {};
  }

  template <std::convertible_to<promiseT> rFrom>
  void return_value(rFrom &&from) {
    return_val = std::forward<rFrom>(from);
  };
  void unhandled_exception() {};
  promise_packed storage;
  promiseT return_val;
};


}; // namespace buffio

#endif
