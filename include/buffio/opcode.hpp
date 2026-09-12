#ifndef BUFFIO_OPCODE
#define BUFFIO_OPCODE

#include "buffio/core.hpp"
#include <utility>

namespace buffio{

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


};

#endif
