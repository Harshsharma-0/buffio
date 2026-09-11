#ifndef BUFFIO_CORE_HPP
#define BUFFIO_CORE_HPP

#include "buffio/config.hpp"
#include <atomic>
#include <cassert>
#include <coroutine>
#include <filesystem>
#include <optional>
#include <utility>
#include <variant>

using BF_PATH_PREFIX = std::filesystem::path;
#if defined(BUFFIO_OS_WINDOWS)
#define BUFFIO_WIDEN_HELPER(str) L##str
#else
#define BUFFIO_WIDEN_HELPER(str) str
#endif

#define BUFFIO_WIDEN(name) BUFFIO_WIDEN_HELPER(name)

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

struct NoOp {
  void action(void *) {}
};

enum class OpCode : int {
  Open = 1,
  Read,
  Write,
  Readv, 
  Writev,
  pRead,
  pWrite,
  pReadv,
  pWritev,
};

struct OpState {
  CoroutineHandle task;
  OpCode op_code;
  union {
    bool (*action)(std::pair<void *, void *>);
  };
  int error;
  union {
    void *data;
    ssize_t op_done;
    buffio_fd fd;
  };
};

#if defined(BUFFIO_OS_WINDOWS)

struct Bufferiov{
 void operator=(char *data){
   iov_base = static_cast<void*>(data);
 };
 void operator=(size_t len){
  iov_len = len;
 };
 void *iov_base;
 size_t iov_len;
};

#elif defined(BUFFIO_OS_LINUX)
using BufferiovBase = struct iovec;
struct Bufferiov: public BufferiovBase{
 void operator=(char *data){
   iov_base = static_cast<void*>(data);
 };
 void operator=(size_t len){
  iov_len = len;
 };

};
#endif

class BuffervState {

public:
  std::pair<Bufferiov*, size_t> get() const { return {io_vecs, size}; };

  bool CreateVec(int num);
  bool MakeEntry(int idx, char *buffer, size_t bufSize);
  BuffervState() = default;

private:
  Bufferiov *io_vecs = nullptr;
  size_t size = 0;
  size_t size_max = 0;
  bool iown = false;
};

/*
class FileStat{
  using FileStatType = BUFFIO_OS_INSERT(
      struct stat,
      struct stat,
      BY_HANDLE_FILE_INFORMATION
      );
  public:
    FileStat() = default;
    ~FileStat() = default;


    //64 bytes for both 64-bit and 32-bit system.
    uint64_t Size() const {
      BUFFIO_OS_INSERT(
       return (uint64_t)stat_buf.st_size; ,
       return (uint64_t)stat_buf.st_size; ,
       return stat_buf.
      }
    };

  private:
    FileStateType stat_buf;

};
*/

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
