#include "buffio/config.hpp"


#ifdef BUFFIO_BACKEND_IOURING
#include "buffio/fs.hpp"
#include "buffio/worker.hpp"
#include "buffio/ecode.hpp"
#include <unistd.h>
#include <fcntl.h>

bool buffio::OpenFileAwaiter::action(std::pair<void*,void*> info){

  auto[p_sqe,p_self] = info;
  buffio::OpenFileAwaiter *obj =
           static_cast<buffio::OpenFileAwaiter *>(p_self);

  struct io_uring_sqe *sqe = 
           static_cast<struct io_uring_sqe *>(p_sqe);
  
  io_uring_prep_open(sqe,
      obj->path,
      obj->flags,
      (mode_t)obj->mode);

  io_uring_sqe_set_data(sqe,(void *)&obj->op_state);
   
  return true;
};


bool buffio::AwaitableFileBase::action(std::pair<void*,void*> info){

 auto[p_sqe,p_self] = info;
 buffio::AwaitableFileBase *obj =
               static_cast<buffio::AwaitableFileBase *>(p_self);

 struct io_uring_sqe *sqe = 
       static_cast<struct io_uring_sqe *>(p_sqe);
 
  auto[fd,buffer,size,offset,poffset] = obj->state;

  uint8_t op = 0;
  uint64_t buffer64 = reinterpret_cast<uint64_t>(buffer);
  uint32_t len = static_cast<uint32_t>(size);

  switch(obj->op_state.op_code){
    case OpCode::Read: 
      op = IORING_OP_READ;
    break;
    case OpCode::Write: 
      op = IORING_OP_WRITE;
    break;
    case OpCode::Readv:
      op = IORING_OP_READV;
    break;
    case OpCode::Writev:
      op = IORING_OP_WRITEV;
    break;
    case OpCode::pRead:
      op = IORING_OP_READ;
    break;
    case OpCode::pWrite:
      op = IORING_OP_WRITE;
    break;
    case OpCode::pReadv:
      op = IORING_OP_READV;
    break;
    case OpCode::pWritev:
      op = IORING_OP_WRITEV;
    break;
    default:
     //error no suitable op found
     assert(false);
    break;
  };
 
  
  io_uring_initialize_sqe(sqe);

  sqe->opcode = op;  
  sqe->fd = fd;
  sqe->addr = buffer64;

  sqe->off = offset;
  sqe->len = len;
  sqe->user_data = reinterpret_cast<uint64_t>(&obj->op_state);


  return true;
};
#endif
