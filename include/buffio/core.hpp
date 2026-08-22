#ifndef BUFFIO_CORE_HPP
#define BUFFIO_CORE_HPP


#include "buffio/config.hpp"
#include <cassert>
#include <coroutine>
#include <utility>
#include <variant>
#include <optional>
#include <filesystem>

using BF_PATH_PREFIX=std::filesystem::path;


using BFILE = buffio_fd;
using BFFDOPT = std::pair<int,int>;
using BFRWOPT = std::pair<ssize_t,int>;
using BFOPT   = std::pair<int,int>;
using BFSRWOPT = std::pair<ssize_t,uintptr_t>;

namespace buffio{


class Worker;

using CoroutineHandle = std::coroutine_handle<>;

struct PromiseState {
    Worker* worker;
    CoroutineHandle waiter;
    bool waiter_available;
    CoroutineHandle self;

};

struct ReadFileAwaiter;
struct ReadvFileAwaiter;
struct WriteFileAwaiter;
struct WritevFileAwaiter;
struct CloseFileAwaiter;

struct ReadFile;
struct ReadvFile;
struct WriteFile;
struct WritevFile;
struct CloseFile;

struct NoOp {
    void action(void*) {}
};

using op_vec =  std::variant<
                 std::monostate,
                 buffio::ReadFileAwaiter*,
                 buffio::ReadvFileAwaiter*,
                 buffio::WriteFileAwaiter*,
                 buffio::WritevFileAwaiter*,
                 buffio::NoOp *
                 >;




struct ReadFile{
 buffio_fd fd;
 char *buffer;
 size_t size;
 size_t *offset;
};

struct ReadvFile{
 buffio_fd fd;
 buffio::ReadFile *buffers;
 size_t size;
 size_t *offset;
};

struct WriteFile{
 buffio_fd fd;
 char *buffer;
 size_t size;
 size_t *offset;
};

struct WritevFile{
  buffio_fd fd;
  buffio::WriteFile *buffers;
  size_t size;
  size_t *offset;
};

struct OpenFile{
  bool await_ready() {return true;}
  void await_suspend(buffio::CoroutineHandle){};
  ssize_t await_resume();

  BF_PATH_PREFIX const &path;
  BFLAG flags;
  BMODE mode;
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
   
   /*
    removed so the tasks can wait for other task also
   inline ReadFileAwaiter await_transform(ReadFile const &fields) const{
     return ReadFileAwaiter{
                    fields,
                    state.self,
                    std::monostate{},
                    state.worker
                   };
   };
   
   inline ReadvFileAwaiter await_transform(ReadvFile const &fields) const{
     return ReadvFileAwaiter{
                     fields,
                     nullptr,
                     std::monostate{},
                     state.worker
                    };
   };

   inline WriteFileAwaiter await_transform(WriteFile const &fields) const{
     return WriteFileAwaiter{
                     fields,
                     nullptr,
                     std::monostate{},
                     state.worker
                    };
   };
   inline WritevFileAwaiter await_transform(WritevFile const &fields) const{
     return WritevFileAwaiter{
                      fields,
                      nullptr,
                      std::monostate{},
                      state.worker
                    };
   };
   */
   TaskFinalSuspendAwaitable final_suspend() noexcept;
   PromiseState state;
};

struct Task {
    bool core_schedule(
        Worker& worker,
        CoroutineHandle task
    );

    bool promise_and_push(
        PromiseState& promise,
        CoroutineHandle task,
        CoroutineHandle self
    );
};

}; // namespace core

};

#endif
