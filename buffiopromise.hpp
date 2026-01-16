#ifndef __BUFFIO_PROMISE_HPP__
#define __BUFFIO_PROMISE_HPP__

/*
* Error codes range reserved for buffiothread
*  [10000 - 11500]
*  10000 <= errorcode <= 10500
*/

#if !defined(BUFFIO_IMPLEMENTATION)
   #include "buffioenum.hpp"
   #include "buffiosock.hpp"
#endif

#include <coroutine>
#include <exception>
#include <atomic>


struct buffiopromise;
using buffiohandleroutine = std::coroutine_handle<buffiopromise>;

#define buffiowait co_await
#define buffioyeild co_yield
#define buffioreturn co_return
#define buffiopush co_await


struct buffioroutine : buffiohandleroutine {
  using promise_type = ::buffiopromise;
};



struct acceptreturn {
  int errorcode;
  buffioroutine handle;
};

struct buffioawaiter {
  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> h) noexcept {};
  buffioroutine await_resume() noexcept { return self; };
  buffioroutine self;
};

struct buffiopushinfo{
  buffioroutine task;
};

struct buffiopromisestatus {
  enum BUFFIO_ROUTINE_STATUS status;
  int returncode = 0;
  std::exception_ptr routineexception;
};

struct buffiopromise {
  buffioroutine waitingfor;
  buffioroutine pushhandle;
  buffioroutine self;
  buffiopromisestatus childstatus;
  buffiopromisestatus selfstatus;

  std::exception_ptr routineexception;

  buffioroutine get_return_object() {
    self = {buffioroutine::from_promise(*this)};
    selfstatus.status = BUFFIO_ROUTINE_STATUS_EXECUTING;
    return self;
  };

  std::suspend_always initial_suspend() noexcept { return {}; };
  std::suspend_always final_suspend() noexcept { return {};};

  std::suspend_always yield_value(int value) {
    selfstatus.status = BUFFIO_ROUTINE_STATUS_YIELD;
    return {};
  };

  buffioawaiter await_transform(buffioroutine waitfor) {
    waitingfor = waitfor;
    selfstatus.status = BUFFIO_ROUTINE_STATUS_WAITING;
    return {.self = self};
  };

  buffioawaiter await_transform(buffiopushinfo info) {
    pushhandle = info.task;
    selfstatus.status = BUFFIO_ROUTINE_STATUS_PUSH_TASK;
    return {.self = info.task};
  };

  // overload to submit I/O request via the promise to the sockbroker
  buffioawaiter await_transform(buffiofd &sockview){
    return {};
  }
  void unhandled_exception() {
    selfstatus.status = BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION;
    selfstatus.returncode = -1;
    selfstatus.routineexception = std::current_exception();
  };

  void return_value(int state) {
    selfstatus.returncode = state;
    selfstatus.status =
        state < 0 ? BUFFIO_ROUTINE_STATUS_ERROR : BUFFIO_ROUTINE_STATUS_DONE;
    return;
  };
  void setstatus(enum BUFFIO_ROUTINE_STATUS stat){ selfstatus.status = stat; }
  bool checkstatus() {
    return selfstatus.status == BUFFIO_ROUTINE_STATUS_ERROR ||
                   selfstatus.returncode < 0
               ? true
               : false;
  };
};

class buffiocatch {

public:
  buffiocatch(buffioroutine self) : evalue(self) {
    status = self.promise().childstatus;
  };
  void exceptionthrower() {
    switch (status.status) {
    case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
      std::rethrow_exception(status.routineexception);
      break;
    case BUFFIO_ROUTINE_STATUS_ERROR:
      throw std::runtime_error(
          "error in execution of routine return code less than 0");
      break;
    case BUFFIO_ROUTINE_STATUS_DONE:
      return;
      break;
    }
  }
  buffiocatch &throwerror() {
    exceptionthrower();
    return *this;
  };

  void operator=(void (*handler)(const std::exception &e, int successcode)) {
    if (evalue.promise().checkstatus()) {
      try {
        exceptionthrower();
      } catch (const std::exception &e) {
        handler(e, status.returncode);
      }
    }
  };

private:
  buffioroutine evalue;
  buffiopromisestatus status;
};

struct buffiotaskinfo{
  std::atomic<int64_t> mask; // don't remove this mask as if tracks if there any request available; 
  size_t id;  //mask track if the task have socket,bucket
  buffioroutine task;
};

#endif 
