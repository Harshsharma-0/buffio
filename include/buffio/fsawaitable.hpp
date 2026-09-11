#ifndef BUFFIO_FS_AWAITABLE_HPP
#define BUFFIO_FS_AWAITABLE_HPP

#include "buffio/core.hpp"

namespace buffio{


struct AwaitableFileBase{
 void await_suspend(CoroutineHandle task_);
 std::pair<ssize_t,int> await_resume(){
    return {op_state.op_done,op_state.error};
  };
 
  static bool action(std::pair<void *,void*> info);
 
  struct{
   buffio_fd fd;
   char *buffer;
   size_t size;
   uint64_t offset;
  #ifdef BUFFIO_BACKEND_IOURING
   uint64_t *poffset;
   #endif
  }state;

  OpState op_state;
};

struct OpenFileAwaiter {
  bool await_ready() {
    op_state.op_code = OpCode::Open;
    op_state.action = OpenFileAwaiter::action;
    op_state.data = static_cast<void *>(this);
    return false; 
  };
  void await_suspend(CoroutineHandle task_);
  int await_resume();

  static bool action(std::pair<void *, void *> info);
 
  #if defined(BUFFIO_OS_LINUX)
  char *path;
  #elif defined(BUFFIO_OS_WINDOWS)
  std::wstring path;
  #endif
  int flags;
  int mode;
  void *rval;
  OpState op_state;
};


struct ReadFileAwaiter : AwaitableFileBase{
  bool await_ready() {
    op_state.op_code = OpCode::Read;
    return false;
  };
};


struct WriteFileAwaiter : AwaitableFileBase{
  bool await_ready() {
    op_state.op_code = OpCode::Write;
    return false;
  };
};

struct ReadvFileAwaiter: AwaitableFileBase {
  bool await_ready() {
    op_state.op_code = OpCode::Readv;
    return false; 
  };
};

struct WritevFileAwaiter: AwaitableFileBase {
  bool await_ready() {
    op_state.op_code = OpCode::Writev;
    return false;
  }
};

struct ReadOffsetFileAwaiter:AwaitableFileBase{
  bool await_ready(){
    op_state.op_code = OpCode::pRead;
    return false;
  };
};

struct ReadvOffsetFileAwaiter:AwaitableFileBase{
  bool await_ready(){
    op_state.op_code = OpCode::pReadv;
   return false;
  };
};

struct WriteOffsetFileAwaiter:AwaitableFileBase{
  bool await_ready(){
    op_state.op_code = OpCode::pWrite;
    return false;
  };
};

struct WritevOffsetAwaitable:AwaitableFileBase{
  bool await_ready(){
    op_state.op_code = OpCode::pWritev;
    return false;
  };
};

struct FsMkDirAwaitable {

  bool await_ready() { 
    if(!async)
      FsMkDirAwaitable::action({nullptr,this});
    return !async;
  }
  void await_suspend(CoroutineHandle task_);
  ssize_t await_resume() const { return op_state.op_done; };

  static bool action(std::pair<void *, void *> info);

  char *path;
  bool async;
  OpState op_state;
};


/* TODO: add fs ops like mkdir/createdir... etc. 
struct AwaitableFsBase{

};

struct FsLinkAwaitable{
  bool await_ready() { 
    if(!async)
      FsLinkAwaitable::action({nullptr,this});
    return !async;
  };

  void await_suspend(CoroutineHandle task_);
  ssize_t await_resume() const { return op_state.op_done; };

  static bool action(std::pair<void *, void *> info);

  char *path;
  bool async;

  BUFFIO_OS_INSERT(OpState op_state, OpState op_state, OVERLAPPED op_state);

};
struct FsUnlinkAwaitable{
  bool await_ready() { 
    if(!async)
      FsUnlinkAwaitable::action({nullptr,this});
    return !async;
  }
  void await_suspend(CoroutineHandle task_);
  ssize_t await_resume() const { return op_state.op_done; };

  static bool action(std::pair<void *, void *> info);

  char *path;
  bool async;

  BUFFIO_OS_INSERT(OpState op_state, OpState op_state, OVERLAPPED op_state);

};
struct FsRenameAwaitable{

  bool await_ready() { 
    if(!async)
      FsRenameAwaitable::action({nullptr,this});
    return !async;
  }
  void await_suspend(CoroutineHandle task_);
  ssize_t await_resume() const { return op_state.op_done; };

  static bool action(std::pair<void *, void *> info);

  char *path;
  bool async;

  BUFFIO_OS_INSERT(OpState op_state, OpState op_state, OVERLAPPED op_state);
};
*/
};
#endif
