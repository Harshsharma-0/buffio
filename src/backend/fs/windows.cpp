#include "buffio/config.hpp"
#include "buffio/fs.hpp"
#include "buffio/worker.hpp"

bool buffio::File::OpenStdIn(){
  assert(fd == BUFFIO_FD_INVALID);
  buffio_fd tmp_fd = GetStdHandle(STD_INPUT_HANDLE);
  if(tmp_fd == INVALID_HANDLE_VALUE)
      return false;
  fd = tmp_fd;
  return true; 
};

bool buffio::File::OpenStdOut(){ 
  assert(fd == BUFFIO_FD_INVALID);
  buffio_fd tmp_fd = GetStdHandle(STD_OUTPUT_HANDLE);
  if(tmp_fd == INVALID_HANDLE_VALUE)
    return false;

  fd = tmp_fd;
  return true;
};


bool buffio::OpenFileAwaiter::action(std::pair<void*,void*> info){
  auto[p_sqe,p_self] = info;
  buffio::OpenFileAwaiter *obj =
           static_cast<buffio::OpenFileAwaiter *>(p_self);
  
           /*
  int fd = open(obj->path,obj->flags,(mode_t)obj->mode); 
  if(fd < 0){
    obj->op_state.fd = BUFFIO_FD_INVALID;
    return true;
  };


  obj->op_state.fd = fd;
  */
  
  return true;
};

bool buffio::AwaitableFileBase::action(std::pair<void*,void*> info){

 auto[p_sqe,p_self] = info;
 buffio::AwaitableFileBase *obj =
               static_cast<buffio::AwaitableFileBase *>(p_self);

 auto[fd,buffer,size,offset] = obj->state;
 ssize_t rval = 0;

 /*
 switch(obj->op_state.op_code){
  case OpCode::Read:  rval = pread(fd,buffer,size,*offset); break;
  case OpCode::Write: rval = pwrite(fd,buffer,size,*offset); break;
  case OpCode::Readv: rval = preadv(fd,(struct iovec*)buffer,size,*offset); break;
  case OpCode::Writev: rval = pwritev(fd,(struct iovec*)buffer,size,*offset); break;
  default: rval = -1; break;
 };
 */
  obj->op_state.op_done = rval;
  return true;
};

