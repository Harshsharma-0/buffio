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

namespace buffio{


class Worker;
class File;

using CoroutineHandle = std::coroutine_handle<>;

struct PromiseState {
    Worker* worker;
    CoroutineHandle waiter;
    bool waiter_available;
    CoroutineHandle self;

};


struct OpenFileAwaiter;
struct ReadFileAwaiter;
struct ReadvFileAwaiter;
struct WriteFileAwaiter;
struct WritevFileAwaiter;
struct CloseFileAwaiter;


struct NoOp {
    void action(void*) {}
};

struct OpState{
  CoroutineHandle task;

 union{
   bool(*action)(std::pair<void*,void*> );
  };
 union{
  void *data;
  int32_t op_done;
  size_t nread;
  size_t nwrite;
  ssize_t s_nread;
  ssize_t s_nwrite;
  intptr_t pfd;
  int fd;
 };
};


struct BufferState{
 buffio_fd fd;
 char *buffer;
 size_t size;
 size_t *offset;
};

class BuffervState{
  using BuffervStateType = BUFFIO_OS_INSERT(struct iovec *, 
                                   void* ,FILE_SEGMENT_ELEMENT *);
 public:
  void operator()(BuffervState const &buff){
    assert(!iown);
    io_vecs = buff.io_vecs;
    size = buff.size;
  };

 void operator()(BuffervState const &&buff){
    assert(!iown);
    io_vecs = buff.io_vecs;
    size = buff.size;
  };
  std::pair<BuffervStateType,int>get() const {
    return {io_vecs,size};
  };
  

  bool CreateVec(int num);
  bool MakeEntry(int idx,char *buffer,uint32_t bufSize);
  BuffervState() = default;

  private:

  BuffervStateType io_vecs = nullptr;
  int size = 0;
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
