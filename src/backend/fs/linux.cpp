#include "buffio/config.hpp"
#include "buffio/fs.hpp"
#include "buffio/worker.hpp"
#include <unistd.h>
#include <fcntl.h>

bool buffio::File::OpenStdIn(){
  assert(fd <= BUFFIO_FD_INVALID);
  fd = STDIN_FILENO;
  return true; 
};

bool buffio::File::OpenStdOut(){ 
  assert(fd <= BUFFIO_FD_INVALID);
  fd = STDOUT_FILENO;
  return true;
};

int buffio::OpenFileAwaiter::await_resume(){ 
  if(op_state.op_done < 0) return -1;

  buffio::File *file = 
       static_cast<buffio::File*>(rval); 

  file->fd = op_state.op_done;
  file->loffset = 0;

  return 0;
};

#ifdef BUFFIO_BACKEND_IOURING


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
 
  auto[fd,buffer,size,offset] = obj->state;

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
    default: 
      return false;
    break;
  };
 
  
  io_uring_initialize_sqe(sqe);

  sqe->opcode = op;  
  sqe->fd = fd;
  sqe->addr = buffer64;

  sqe->off = *offset;
  sqe->len = static_cast<uint32_t>(size);
  sqe->user_data = reinterpret_cast<uint64_t>(&obj->op_state);


  return true;
};

#elif defined(BUFFIO_BACKEND_EPOLL)

/* BELOW CODE FOR NON-IO URING */

bool buffio::OpenFileAwaiter::action(std::pair<void*,void*> info){
  auto[p_sqe,p_self] = info;
  buffio::OpenFileAwaiter *obj =
           static_cast<buffio::OpenFileAwaiter *>(p_self);

  int fd = open(obj->path,obj->flags,(mode_t)obj->mode); 
  obj->op_state.op_done = fd;
  return true;
};

bool buffio::AwaitableFileBase::action(std::pair<void*,void*> info){

 auto[p_sqe,p_self] = info;
 buffio::AwaitableFileBase *obj =
               static_cast<buffio::AwaitableFileBase *>(p_self);

 auto[fd,buffer,size,offset] = obj->state;
 ssize_t rval = 0;

 switch(obj->op_state.op_code){
  case OpCode::Read:  rval = pread(fd,buffer,size,*offset); break;
  case OpCode::Write: rval = pwrite(fd,buffer,size,*offset); break;
  case OpCode::Readv: rval = preadv(fd,(struct iovec*)buffer,size,*offset); break;
  case OpCode::Writev: rval = pwritev(fd,(struct iovec*)buffer,size,*offset); break;
  default: rval = -1; break;
 };
  obj->op_state.op_done = rval;
  return true;
};

#else
 #error file src/backend/fs/linuc.cpp cannot see BACKEND macro defination
#endif 


