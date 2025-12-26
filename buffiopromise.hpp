#ifndef __BUFFIO_PROMISE_HPP__
#define __BUFFIO_PROMISE_HPP__

#include <coroutine>
#include <exception>

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

  std::suspend_always initial_suspend() noexcept { 
    std::cout<<"suspended initial"<<std::endl;
    return {}; 
  };
  std::suspend_always final_suspend() noexcept { 
    std::cout<<"suspended final"<<std::endl;
    return {};
  };
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
    selfstatus.status = BUFFIO_ROUTINE_STATUS_PAUSED;
    return {.self = info.task};
  };

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

#endif 
