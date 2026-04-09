#ifndef __BUFFIO_PROMISE_HPP__
#define __BUFFIO_PROMISE_HPP__

#include "buffio/enum.hpp"
#include "buffio/fd.hpp"
#include "buffio/common.hpp"
#include "buffio/fiber.hpp"
#include <cassert>
#include <exception>
#include <type_traits>

#define buffiowait co_await
#define buffioyeild co_yield
#define buffioreturn co_return
#define buffiopush co_await

struct buffioAwaiter {
  bool await_ready() const noexcept { return ready; }
  void await_suspend(std::coroutine_handle<> h) noexcept {};
  std::coroutine_handle<> await_resume() noexcept { return NULL; };
  bool ready;
};

namespace buffio {
class promise {
public:
  class promise_type;
  using coro_handle = std::coroutine_handle<promise_type>;
  using void_handle = std::coroutine_handle<>;

  class promise_type {
    friend buffio::promise;

  private:
    buffioRoutineStatus status = buffioRoutineStatus::fresh;
 
  public:
    using promiseObject = promise::promise_type;
    using buffioTypedHandle = std::coroutine_handle<promiseObject>;
    using promiseHandle = std::coroutine_handle<>;

    inline promiseObject *getPromise(promiseHandle handle) {
      void *tmp_ptr = handle.address();
      buffioTypedHandle typed = buffioTypedHandle::from_address(tmp_ptr);
      return &typed.promise();
    };

    void_handle get_return_object() {
         return {coro_handle::from_promise(*this)};
    };

    std::suspend_always initial_suspend() noexcept {
      status = buffioRoutineStatus::executing;
      return {};
    };

    std::suspend_always final_suspend() noexcept { return {}; };
    std::suspend_always yield_value(int value) { return {}; };

    buffioAwaiter await_transform(promise promise);
    buffioAwaiter await_transform(buffioRoutineStatus ustatus) const;
    buffioAwaiter await_transform(buffio::clockSpec::wait wait);
    buffioAwaiter await_transform(buffioHeader *header);
    buffioAwaiter await_transform(fiber::clampInfo info);

    void unhandled_exception() {
      status = buffioRoutineStatus::unhandledException;
    };

    void return_value(int state) {
      status = buffioRoutineStatus::done;
      return;
    };

  };

  promise(void_handle _handle) : handle(_handle) {
    assert(_handle);
    paddr = &promise::promise_type::buffioTypedHandle::from_address(
                 _handle.address())
                 .promise();
  }

  void_handle get() const { return handle; };
  static void run(void *data);
  static void destroy(void *data);
  static buffioRoutineStatus checkStatus(void *data);
  static buffioHeader *getHeader(void *data);
  static void setStatus(void *data,buffioRoutineStatus status);

private:
  void_handle handle;
  promise::promise_type *paddr;
};

using promiseObject = buffio::promise::promise_type::promiseObject;
using buffioTypedHandle = buffio::promise::promise_type::buffioTypedHandle;

inline promiseObject *getPromise(promiseHandle handle) {
  void *tmp_ptr = handle.address();
  buffioTypedHandle typed = buffioTypedHandle::from_address(tmp_ptr);
  return &typed.promise();
};

}; // namespace buffio

#endif
