#ifndef BUFFIO_CORE_HPP
#define BUFFIO_CORE_HPP

#include "buffio/config.hpp"

#include <cassert>
#include <coroutine>
#include <filesystem>


namespace buffio {

class Worker;
class File;

using CoroutineHandle = std::coroutine_handle<>;

struct PromiseState {
  Worker *worker;
  CoroutineHandle waiter;
  bool waiter_available;
  CoroutineHandle self;
};


struct TaskFinalSuspendAwaitable {
  bool await_ready() noexcept { return ready; };
  void await_suspend(buffio::CoroutineHandle) noexcept {}
  void await_resume() noexcept {}
  bool ready;
};

namespace core {

class Awaitable {};

class Promise {
public:
  TaskFinalSuspendAwaitable final_suspend() noexcept;
  PromiseState state;
};

struct Task {
  bool core_schedule(Worker &worker, CoroutineHandle task);

  bool promise_and_push(PromiseState &promise, CoroutineHandle task,
                        CoroutineHandle self);
};

}; // namespace core

}; // namespace buffio

#endif
