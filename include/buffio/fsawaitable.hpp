#ifndef BUFFIO_FS_AWAITABLE_HPP
#define BUFFIO_FS_AWAITABLE_HPP

#include "buffio/bufferv.hpp"
#include "buffio/core.hpp"
#include "buffio/opcode.hpp"

namespace buffio {

struct AwaitableFileBase {
  void await_suspend(CoroutineHandle task_);
  std::pair<ssize_t, int> await_resume() {
    return {op_state.op_done, op_state.error};
  };

  static bool action(std::pair<void *, void *> info);

  struct {
    buffio_fd fd;
    char *buffer;
    size_t size;
    uint64_t offset;
#ifdef BUFFIO_BACKEND_IOURING
    uint64_t *poffset;
#endif
  } state;

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

struct ReadFileAwaiter : AwaitableFileBase {
  bool await_ready() {
    op_state.op_code = OpCode::Read;
    return false;
  };
};

struct WriteFileAwaiter : AwaitableFileBase {
  bool await_ready() {
    op_state.op_code = OpCode::Write;
    return false;
  };
};

struct ReadvFileAwaiter : AwaitableFileBase {
  bool await_ready() {
    op_state.op_code = OpCode::Readv;
    return false;
  };
};

struct WritevFileAwaiter : AwaitableFileBase {
  bool await_ready() {
    op_state.op_code = OpCode::Writev;
    return false;
  }
};

struct ReadOffsetFileAwaiter : AwaitableFileBase {
  bool await_ready() {
    op_state.op_code = OpCode::pRead;
    return false;
  };
};

struct ReadvOffsetFileAwaiter : AwaitableFileBase {
  bool await_ready() {
    op_state.op_code = OpCode::pReadv;
    return false;
  };
};

struct WriteOffsetFileAwaiter : AwaitableFileBase {
  bool await_ready() {
    op_state.op_code = OpCode::pWrite;
    return false;
  };
};

struct WritevOffsetAwaitable : AwaitableFileBase {
  bool await_ready() {
    op_state.op_code = OpCode::pWritev;
    return false;
  };
};

/* INPROGRESS: add fs ops like mkdir/createdir... etc. */
struct AwaitableFsBase {
  void await_suspend(CoroutineHandle task_) {};
  int await_resume() const { return op_state.error; };
  static bool action(std::pair<void *, void *> info);

  OpState op_state;
};

struct FsMkDirAwaitable : AwaitableFsBase {
  bool await_ready() { return false; }

};

struct FsLinkAwaitable:AwaitableFsBase {
  bool await_ready() { return false; }
};

struct FsUnlinkAwaitable:AwaitableFsBase {
  bool await_ready() { return false; }

};
struct FsRenameAwaitable:AwaitableFsBase {
  bool await_ready() { return false; }
};

}; // namespace buffio
#endif
