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
    default: 
      return false;
    break;
  };
 
  
  io_uring_initialize_sqe(sqe);

  sqe->opcode = op;  
  sqe->fd = fd;
  sqe->addr = buffer64;

  sqe->off = offset;
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
  if(fd < 0){
    obj->op_state.fd = BUFFIO_FD_INVALID;
    return true;
  };

  obj->op_state.fd = fd;
  
  return true;
};

bool buffio::AwaitableFileBase::action(std::pair<void*,void*> info){

 auto[p_sqe,p_self] = info;
 buffio::AwaitableFileBase *obj =
               static_cast<buffio::AwaitableFileBase *>(p_self);

 auto[fd,buffer,size,offset] = obj->state;
 ssize_t rval = 0;

 switch(obj->op_state.op_code){
  case OpCode::Read:  rval = read(fd,buffer,size); break;
  case OpCode::Write: rval = write(fd,buffer,size); break;
  case OpCode::Readv: rval = readv(fd,(struct iovec*)buffer,size); break;
  case OpCode::Writev: rval = writev(fd,(struct iovec*)buffer,size); break;
  case OpCode::pRead: rval = pread(fd,buffer,size,offset); break;
  case OpCode::pWrite: rval = pwrite(fd,buffer,size,offset); break;
  case OpCode::pReadv: rval = preadv(fd,(struct iovec*)buffer,size,offset); break;
  case OpCode::pWritev: rval = pwritev(fd,(struct iovec*)buffer,size,offset); break;
  default: rval = -1; break;
 };
  
 obj->op_state.op_done = rval;

  return true;
};

#else
 #error file src/backend/fs/linuc.cpp cannot see BACKEND macro defination
#endif 


