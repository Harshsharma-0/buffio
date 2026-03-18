#ifndef __BUFFIO_PROMISE_HPP__
#define __BUFFIO_PROMISE_HPP__

#include "buffio/enum.hpp"
#include "buffio/fd.hpp"
#include "common.hpp"
#include "fiber.hpp"
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
    buffioRoutineStatus status = buffioRoutineStatus::fresh;

    void_handle handle_child = nullptr;
    void_handle voidSelf = nullptr;

    bool threaded = false;
    uintptr_t auxData = 0;

  public:
    T returnData;

    void_handle get_return_object() {
      voidSelf = {coro_handle::from_promise(*this)};
      return voidSelf;
    };
    void setChild(auto child) noexcept { handle_child = child; }
    void_handle getChild() const { return handle_child; }
    // initially called when the routine is framed
    std::suspend_always initial_suspend() noexcept {
      status = buffioRoutineStatus::executing;
      return {};
    };

    std::suspend_always final_suspend() noexcept { return {}; };
    std::suspend_always yield_value(int value) { return {}; };

    template <typename P> buffioAwaiter await_transform(P handle) {
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
        if (handle == buffioRoutineStatus::none)
          continueRoutine = true;
        status = handle;

      } else if constexpr (std::is_same_v<P, buffio::clockSpec::wait>) {
        buffio::fiber::timerClock->push(handle.ms, voidSelf);
        status = buffioRoutineStatus::waitingTimer;

      } else if constexpr (std::is_same_v<P, buffioHeader *>) {
        if (handle == nullptr)
          return {.self = handleTmp, .ready = true};

        handle->routine = voidSelf;
        status = handle->fd->getFamily() == buffioFdFamily::file
                     ? buffioRoutineStatus::waitingFile
                     : buffioRoutineStatus::waitingFd;
      } else if constexpr (std::is_same_v<P, fiber::clampInfo>) {
        if (handle.header == nullptr)
          return {.self = voidSelf, .ready = true};

        auxData = (uintptr_t)handle.header;
        handle.header->then = handle.header->routine;
        handle.header->routine = voidSelf;
        threaded = true;
        status = buffioRoutineStatus::clampThread;
        return {.self = voidSelf, .ready = false};

      } else {
        static_assert(false, "we don't support this type of call now");
      };

      return {.self = handleTmp, .ready = continueRoutine};
    };
    bool isThreaded() const { return threaded; }
    void setAux(uintptr_t data, bool thr) {
      auxData = data;
      threaded = thr;
    }
    template <typename auxType> auxType getAux() const {
      return (auxType)auxData;
    }
    void unhandled_exception() {
      status = buffioRoutineStatus::unhandledException;
    };

    void return_value(T state) {
      returnData = state;
      status = threaded ? buffioRoutineStatus::backFromThread
                        : buffioRoutineStatus::wakeParent;
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
