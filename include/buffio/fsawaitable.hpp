#ifndef BUFFIO_FS_AWAITABLE_HPP
#define BUFFIO_FS_AWAITABLE_HPP

#include "buffio/bufferv.hpp"
#include "buffio/core.hpp"
#include "buffio/opcode.hpp"

namespace buffio {
using Path =  std::filesystem::path;

struct AwaitableFileBase {
  void await_suspend(CoroutineHandle task_);
  
  std::pair<ssize_t, int> await_resume() {
    return {op_state.op_done, op_state.error};
  };

  static bool action(buffio::OpState *state);

  struct {
    buffio_fd fd;
    char *buffer;
    size_t size;
    uint64_t offset;
    uint64_t *poffset;
  } state;

  OpState op_state;
};

struct OpenFileAwaiter {

  bool await_ready() { return false; };
  void await_suspend(CoroutineHandle task_);
  int await_resume();
  
  /* function that actually does the work */
  static bool action(buffio::OpState *state);

  buffio::Path &path;

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

  void await_suspend(CoroutineHandle task_);
  int await_resume() const { return op_state.error; };
  static bool action(buffio::OpState *state);


  buffio::Path &path;
  int mode_t;
  buffio::Path &path_old;
  OpState op_state;
};

struct FsMkDirAwaitable : AwaitableFsBase {
  bool await_ready() { 
    op_state.op_code = buffio::OpCode::MkDir;
    return false; 
  };
};

struct FsLinkAwaitable:AwaitableFsBase {
  bool await_ready() { 
    op_state.op_code = buffio::OpCode::Link;
    return false; 
  };
};

struct FsUnlinkAwaitable:AwaitableFsBase {
  bool await_ready() { 
    op_state.op_code = buffio::OpCode::UnLink;
    return false; 
  };
};

struct FsRenameAwaitable:AwaitableFsBase {
  bool await_ready() { 
    op_state.op_code = buffio::OpCode::Rename;
    return false; 
  };
};

}; // namespace buffio
#endif
