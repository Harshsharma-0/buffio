#ifndef BUFFIO_OPCODE
#define BUFFIO_OPCODE

#include "buffio/config.hpp"
#include "buffio/core.hpp"
#include <utility>

namespace buffio{

enum class OpClass : uint16_t{
 OpenOp = 1,
 FileIo,
 FsOps
};

enum class OpCode : uint16_t {
  Open = 1,
  Read,
  Write,
  Readv, 
  Writev,
  pRead,
  pWrite,
  pReadv,
  pWritev,

  MkDir,
  MkDirAt,
  Rename,
  RenameAt,
  Link,
  LinkAt,
  UnLink,
  UnlinkAt

};

struct OpState {
  CoroutineHandle task;

  struct{
    OpClass op_class;
    OpCode op_code;
  };

  int error;

  #ifdef BUFFIO_BACKEND_IOURING
  struct io_uring_sqe *sqe;
  #endif

  union {
    void *data;
    ssize_t op_done;
    buffio_fd fd;
  };
};


};

#endif
