#ifndef BUFFIO_TASK_CONTAINER
#define BUFFIO_TASK_CONTAINER
#include "buffio/core.hpp"

namespace buffio {

template <typename promiseT> class promise;

template <typename taskT>
struct task : std::coroutine_handle<promise<taskT>>, core::Task {
  using promise_type = buffio::promise<taskT>;

  bool schedule(buffio::Worker &worker) {
    this->promise().state.worker = &worker;
    return core_schedule(worker, {this->from_address(this->address())});
  }

  bool await_ready() { return false; }
  bool await_suspend(buffio::CoroutineHandle waiting_task) {

    // if scheduling failed, immeadiately return control to the caller
    return promise_and_push(this->promise().state, waiting_task,
                            {this->from_address(this->address())});
  };

  taskT await_resume() noexcept {
    taskT rval = this->promise().return_val;
    this->destroy();
    return rval;
  };
};

template <typename promiseT> class promise : public buffio::core::Promise {
public:
  task<promiseT> get_return_object() {
    state.waiter_available = false;
    state.self = {task<promiseT>::from_promise(*this)};
    return {task<promiseT>::from_promise(*this)};
  };

  std::suspend_always initial_suspend() noexcept { return {}; };

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

  promiseT return_val;
};

}; // namespace buffio

#endif
