#include "buffio/config.hpp"

#if defined(BUFFIO_BACKEND_EPOLL)

#include "buffio/fs.hpp"
#include "buffio/worker.hpp"
#include "buffio/ecode.hpp"
#include <unistd.h>
#include <fcntl.h>


bool buffio::OpenFileAwaiter::action(buffio::OpState *state){
  
  buffio::OpenFileAwaiter *obj =
           static_cast<buffio::OpenFileAwaiter *>(state->data);
  
  int fd = open(obj->path.c_str(),
                obj->flags,
                (mode_t)obj->mode); 
  if(fd < 0){
    obj->op_state.error = buffio::error_from_os(errno);
    obj->op_state.fd = BUFFIO_FD_INVALID;
    return true;
  };

  obj->op_state.fd = fd;
  
  return true;
};

bool buffio::AwaitableFileBase::action(buffio::OpState *state){

 buffio::AwaitableFileBase *obj = 
             static_cast<buffio::AwaitableFileBase *>(state->data);

 auto[fd,buffer,size,offset,poffset] = obj->state;

 obj->op_state.error = 0;
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
  default: rval = buffio::B_EUNKNOWN; break;
 };
 
 if(rval < 0){
    state->error = buffio::error_from_os(static_cast<int>(errno));
 };

  state->op_done = rval;

  return true;
};

bool buffio::AwaitableFsBase::action(buffio::OpState *state){
  
  int error = 0;
  buffio::OpCode op = state->op_code;
  buffio::AwaitableFsBase *obj = 
                static_cast<buffio::AwaitableFsBase*>(state->data);

  if(op == buffio::OpCode::MkDir)
       error = mkdir(obj->path.c_str(),obj->mode_t);

  if(op == buffio::OpCode::Rename)
       error = rename(obj->path.c_str(),obj->path_old.c_str());

  if(error < 0)
      error = buffio::error_from_os(errno);

  state->error = error;
  return true;
};
#endif


