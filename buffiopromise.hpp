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
#include <type_traits>
#include <exception>

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

template <typename T> struct buffioPromise {

  struct promise_type;
  using coro_handle = std::coroutine_handle<promise_type>;
  using void_handle = std::coroutine_handle<>;

  struct promise_type {

     template <typename Y>
     using buffioPromiseObject = buffioPromise<Y>::promise_type;
     template <typename U>
     using buffioTypedHandle = std::coroutine_handle<buffioPromiseObject<U>>;
     using buffioHeaderType = buffioHeader;

  private:
    // declaration order-locked
    buffioRoutineStatus status = buffioRoutineStatus::fresh;
    void_handle handle_child = nullptr;
    void_handle voidSelf = nullptr;
    buffioHeaderType *pending = nullptr;
    buffioClock *clock = nullptr;
    buffioSockBroker *broker = nullptr;
    // declaration order-unlocked;
    void killChild(){
      if(handle_child){
        auto *tmp = handle_child.address();
        buffioTypedHandle<char> handle = buffioTypedHandle<char>::from_address(tmp);
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
    void_handle getChild() const {return handle_child;}
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
       if constexpr (std::is_same_v<P, buffioTimer*>) {
          clock->push(handle->duration,voidSelf);
          status = buffioRoutineStatus::waitingTimer;
          return {.self = voidSelf, .ready = false}; 

       } else if constexpr (std::is_same_v<P, buffioHeader*>) {
         broker->push(handle);
         return {.self = voidSelf, .ready = false}; 
       } else if constexpr(std::is_same_v<P,std::coroutine_handle<>>) {
           auto *promise = getPromise<char>(handle);
           promise->setInstance(clock,broker);
           handle_child = handle;
           status = buffioRoutineStatus::waiting;
           return {.self = handle_child,.ready = false};
      }else{
        static_assert(false,"we don't support this type");
       };
       
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
  
    void setInstance(buffioClock *clk , buffioSockBroker *brok){
      this->clock = clk;
      this->broker = brok;
    };
    void setStatus(buffioRoutineStatus stat) { status = stat; }
    buffioRoutineStatus checkStatus() const { return status; };
  };
  buffioPromise(void_handle _handle) : handle(_handle) { assert(_handle); }
  void_handle get() const { return handle; };

  // For simplicity, declare these 4 special functions as deleted:
  buffioPromise(buffioPromise const &) = delete;
  buffioPromise(buffioPromise &&) = delete;
  buffioPromise &operator=(buffioPromise const &) = delete;
  buffioPromise &operator=(buffioPromise &&) = delete;

private:
  void_handle handle;
};

template <typename Y>
using buffioPromiseObject = buffioPromise<Y>::promise_type;
template <typename U>
using buffioTypedHandle = std::coroutine_handle<buffioPromiseObject<U>>;

using buffioPromiseHandle = std::coroutine_handle<>;


template <typename D>
inline buffioPromiseObject<D> *getPromise(buffioPromiseHandle handle) {
  void *tmp_ptr = handle.address();
  buffioTypedHandle<D> typed = buffioTypedHandle<D>::from_address(tmp_ptr);
  return &typed.promise();
};

template <typename G>
constexpr G getReturn(buffioPromiseHandle handle) {
  auto tmp = getPromise<G>(handle);  
  G data = tmp->returnData;
  tmp->setStatus(buffioRoutineStatus::done);
  return data;
};

#endif
