#ifndef BUFFIO_FS_AWAITABLE_HPP
#define BUFFIO_FS_AWAITABLE_HPP

#include "buffio/core.hpp"

namespace buffio{


struct AwaitableFileBase{

 void await_suspend(CoroutineHandle task_);
  ssize_t await_resume() {
    if (op_state.op_done > 0)
      *state.offset += op_state.op_done;
    return op_state.op_done;
  };

  BufferState state; 
  BUFFIO_OS_INSERT(OpState op_state, OpState op_state, OVERLAPPED op_state);
};

struct AwaitableFilevBase{

 void await_suspend(CoroutineHandle task_);
 ssize_t await_resume() const {
    if (op_state.op_done > 0)
      *offset += op_state.op_done;
    return op_state.op_done;
  };

  buffio_fd fd;
  BuffervState state;
  size_t *offset;
  BUFFIO_OS_INSERT(OpState op_state, OpState op_state, OVERLAPPED op_state);

};

struct OpenFileAwaiter {
  bool await_ready() {
    op_state.action = OpenFileAwaiter::action;
    op_state.data = static_cast<void *>(this);
    return false; 
  };
  void await_suspend(CoroutineHandle task_);
  int await_resume();

  static bool action(std::pair<void *, void *> info);

  char *path;
  int flags;
  int mode;
  void *rval;

  BUFFIO_OS_INSERT(OpState op_state, OpState op_state, OVERLAPPED op_state);
};

struct ReadFileAwaiter : AwaitableFileBase {

  bool await_ready() { 
    op_state.action = ReadFileAwaiter::action;
    op_state.data = static_cast<void *>(this);
    return false;
  }
//  void await_suspend(CoroutineHandle task_);
  static bool action(std::pair<void *, void *> info);

};


struct WriteFileAwaiter : AwaitableFileBase{
  bool await_ready() {
    op_state.action = WriteFileAwaiter::action;
    op_state.data = static_cast<void *>(this);
    return false;
  }
//  void await_suspend(CoroutineHandle task_);
  static bool action(std::pair<void *, void *> info);

};

struct ReadvFileAwaiter: AwaitableFilevBase {
  bool await_ready() {
    op_state.action = ReadvFileAwaiter::action;
    op_state.data = static_cast<void *>(this);
    return false; 
  };
//  void await_suspend(CoroutineHandle task_); 
  static bool action(std::pair<void *, void *> info);

};

struct WritevFileAwaiter: AwaitableFilevBase {

  bool await_ready() {
    op_state.action = WritevFileAwaiter::action;
    op_state.data = static_cast<void *>(this);
    return false;
  }
//  void await_suspend(CoroutineHandle task_);
 
  static bool action(std::pair<void *, void *> info);
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

  BUFFIO_OS_INSERT(OpState op_state, OpState op_state, OVERLAPPED op_state);
};


/* TODO 
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
