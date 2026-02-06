#ifndef __BUFFIO_PROMISE_HPP__
#define __BUFFIO_PROMISE_HPP__

#include "common.hpp"
#include "enum.hpp"
#include "fd.hpp"

#include "./sockbroker.hpp"
#include <atomic>
#include <cassert>
#include <coroutine>
#include <exception>
#include <iostream>
#include <type_traits>

#define buffiowait co_await
#define buffioyeild co_yield
#define buffioreturn co_return
#define buffiopush co_await

struct buffioAwaiter {
  bool await_ready() const noexcept { return ready; }
  void await_suspend(std::coroutine_handle<> h) noexcept {};
  std::coroutine_handle<> await_resume() noexcept { return self; };
  std::coroutine_handle<> self;
  bool ready;
};

namespace buffio {
template <typename T> struct promise {

  struct promise_type;
  using coro_handle = std::coroutine_handle<promise_type>;
  using void_handle = std::coroutine_handle<>;

  struct promise_type {

    template <typename Y> using promiseObject = promise<Y>::promise_type;
    template <typename U>
    using buffioTypedHandle = std::coroutine_handle<promiseObject<U>>;

    using promiseHandle = std::coroutine_handle<>;
    template <typename D>

    inline promiseObject<D> *getPromise(promiseHandle handle) {
      void *tmp_ptr = handle.address();
      buffioTypedHandle<D> typed = buffioTypedHandle<D>::from_address(tmp_ptr);
      return &typed.promise();
    };

  private:
    // declaration order-locked
    buffioRoutineStatus status = buffioRoutineStatus::fresh;
    void_handle handle_child = nullptr;
    void_handle voidSelf = nullptr;
    void killChild() {
      if (handle_child) {
        auto *tmp = handle_child.address();
        buffioTypedHandle<char> handle =
            buffioTypedHandle<char>::from_address(tmp);
        handle.promise().setStatus(buffioRoutineStatus::done);
        handle_child = nullptr;
      }
    };

  public:
    T returnData;

    void_handle get_return_object() {
      voidSelf = {coro_handle::from_promise(*this)};
      return voidSelf;
    };
    void_handle getChild() const { return handle_child; }
    // initially called when the routine is framed
    std::suspend_always initial_suspend() noexcept {
      status = buffioRoutineStatus::executing;
      return {};
    };

    std::suspend_always final_suspend() noexcept { return {}; };
    std::suspend_always yield_value(int value) {
      killChild();
      status = buffioRoutineStatus::yield;
      return {};
    };

    template <typename P> buffioAwaiter await_transform(P handle) {
      killChild();
      std::coroutine_handle<> handleTmp = voidSelf;
      bool continueRoutine = false;

      if constexpr (std::is_same_v<P, std::coroutine_handle<>>) {
        handle_child = handle;
        status = buffioRoutineStatus::waiting;
        handleTmp = handle_child;
      } else if constexpr (std::is_same_v<P, promise>) {
        handle_child = handle.get();
        status = buffioRoutineStatus::waiting;
        handleTmp = handle_child;
      } else if constexpr (std::is_same_v<P, buffioRoutineStatus>) {
        switch (handle) {
        case buffioRoutineStatus::none:
          continueRoutine = true;
          break;
        case buffioRoutineStatus::waitingFd:
          status = handle;
          break;
        };
        static_assert(false);
      } else {
        static_assert(false);
      };

      return {.self = handleTmp, .ready = continueRoutine};
    };

    void unhandled_exception() {
      status = buffioRoutineStatus::unhandledException;
      killChild();
    };

    void return_value(T state) {
      returnData = state;
      status = buffioRoutineStatus::wakeParent;
      killChild();
      return;
    };

    void setStatus(buffioRoutineStatus stat) { status = stat; }
    buffioRoutineStatus checkStatus() const { return status; };
  };
  promise(void_handle _handle) : handle(_handle) { assert(_handle); }
  void_handle get() const { return handle; };

  // For simplicity, declare these 4 special functions as deleted:
  promise(promise const &) = delete;
  promise(promise &&) = delete;
  promise &operator=(promise const &) = delete;
  promise &operator=(promise &&) = delete;

private:
  void_handle handle;
};

template <typename Y> using promiseObject = buffio::promise<Y>::promise_type;
template <typename U>
using buffioTypedHandle = std::coroutine_handle<promiseObject<U>>;

template <typename D>
inline promiseObject<D> *getPromise(promiseHandle handle) {
  void *tmp_ptr = handle.address();
  buffioTypedHandle<D> typed = buffioTypedHandle<D>::from_address(tmp_ptr);
  return &typed.promise();
};

template <typename G> constexpr G getReturn(promiseHandle handle) {
  auto tmp = getPromise<G>(handle);
  G data = tmp->returnData;
  tmp->setStatus(buffioRoutineStatus::done);
  return data;
};

}; // namespace buffio

#endif
