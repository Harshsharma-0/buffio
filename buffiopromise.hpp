#ifndef __BUFFIO_PROMISE_HPP__
#define __BUFFIO_PROMISE_HPP__

/*
 * Error codes range reserved for buffiothread
 *  [10000 - 11500]
 *  10000 <= errorcode <= 10500
 */

#if !defined(BUFFIO_IMPLEMENTATION)
#include "buffioenum.hpp"
#include "buffiofd.hpp"
#endif

#include <atomic>
#include <cassert>
#include <coroutine>
#include <exception>
#define buffiowait co_await
#define buffioyeild co_yield
#define buffioreturn co_return
#define buffiopush co_await

template <typename T> struct buffioPromise {

  struct promise_type;
  using coro_handle = std::coroutine_handle<promise_type>;

  struct buffioAwaiter {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept {};
    coro_handle await_resume() noexcept { return self; };
    coro_handle self;
  };
  struct promise_type {
    coro_handle handle;
    coro_handle self;
    buffioRoutineStatus status;
    T returnData;

    coro_handle get_return_object() {
      self = {coro_handle::from_promise(*this)};
      return self;
    };

    // initially called when the routine is framed
    std::suspend_always initial_suspend() noexcept {
      status = buffioRoutineStatus::executing;
      return {};
    };

    std::suspend_always final_suspend() noexcept { return {}; };
    std::suspend_always yield_value(int value) {
      status = buffioRoutineStatus::yield;
      return {};
    };

    buffioAwaiter await_transform(buffioPromise instance) {
      handle = instance.get();
      status = buffioRoutineStatus::waiting;
      return {.self = handle};
    };

    void unhandled_exception() {
      status = buffioRoutineStatus::unhandledException;
    };

    void return_value(T state) {
      returnData = state;
      status = buffioRoutineStatus::done;
      return;
    };
    void setStatus(buffioRoutineStatus stat) { status = stat; }
    bool checkStatus() const { return true; };
  };
  buffioPromise(coro_handle _handle) : handle(_handle) { assert(_handle); }
  coro_handle get() const { return handle; };
  // For simplicity, declare these 4 special functions as deleted:
  buffioPromise(buffioPromise const &) = delete;
  buffioPromise(buffioPromise &&) = delete;
  buffioPromise &operator=(buffioPromise const &) = delete;
  buffioPromise &operator=(buffioPromise &&) = delete;

private:
  coro_handle handle;
};

template <typename U>
using buffioPromiseObject = buffioPromise<U>::promise_type;

template <typename T>
using buffioPromiseHandle = std::coroutine_handle<buffioPromiseObject<T>>;

#endif
