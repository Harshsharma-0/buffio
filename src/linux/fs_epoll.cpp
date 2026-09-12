#include "buffio/config.hpp"

#if defined(BUFFIO_BACKEND_EPOLL)

#include "buffio/fs.hpp"
#include "buffio/worker.hpp"
#include "buffio/ecode.hpp"
#include <unistd.h>
#include <fcntl.h>


bool buffio::OpenFileAwaiter::action(std::pair<void*,void*> info){
  auto[p_sqe,p_self] = info;
  buffio::OpenFileAwaiter *obj =
           static_cast<buffio::OpenFileAwaiter *>(p_self);

  int fd = open(obj->path,obj->flags,(mode_t)obj->mode); 
  if(fd < 0){
    obj->op_state.error = buffio::error_from_os(errno);
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
   obj->op_state.error = buffio::error_from_os(static_cast<int>(errno));
 };

  obj->op_state.op_done = rval;

  return true;
};

#endif


