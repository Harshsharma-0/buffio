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

#ifdef BUFFIO_WORKER_IOURING_HPP


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

bool buffio::ReadFileAwaiter::action(std::pair<void *,void*> info){

  auto[p_sqe,p_self] = info;

  buffio::ReadFileAwaiter *obj =
       static_cast<buffio::ReadFileAwaiter *>(p_self);

  struct io_uring_sqe *sqe = 
       static_cast<struct io_uring_sqe *>(p_sqe);
 
  auto[fd,buffer,size,offset] = obj->state;

  io_uring_prep_read(sqe,
            fd,buffer,size,*offset);
  io_uring_sqe_set_data(sqe,(void *)&obj->op_state);
  

 return true;
};

bool buffio::WriteFileAwaiter::action(std::pair<void*,void*> info){


  auto[p_sqe,p_self] = info;
  buffio::ReadFileAwaiter *obj =
       static_cast<buffio::ReadFileAwaiter *>(p_self);

  struct io_uring_sqe *sqe = 
       static_cast<struct io_uring_sqe *>(p_sqe);
 
  auto[fd,buffer,size,offset] = obj->state;
 
  io_uring_prep_write(sqe,
           fd,buffer,size,*offset);
  io_uring_sqe_set_data(sqe,(void *)&obj->op_state);

 return true;
};

#include <iostream>
bool buffio::ReadvFileAwaiter::action(std::pair<void*,void*> info){

  auto[p_sqe,p_self] = info;
  buffio::ReadvFileAwaiter *obj = 
       static_cast<buffio::ReadvFileAwaiter *>(p_self);

  int fd = obj->fd;
  auto[buffer,size] = obj->state.get();
  size_t offset = *(obj->offset);
  struct io_uring_sqe *sqe = 
      static_cast<struct io_uring_sqe *>(p_sqe);
  
  io_uring_prep_readv(sqe,fd,buffer,1,offset);
  io_uring_sqe_set_data(sqe,(void *)&obj->op_state);

  return true;
};


bool buffio::WritevFileAwaiter::action(std::pair<void*,void*> info){

  auto[p_sqe,p_self] = info;
  buffio::WritevFileAwaiter *obj =
       static_cast<buffio::WritevFileAwaiter *>(p_self);


  int fd = obj->fd;
  auto[buffer,size] = obj->state.get();
  size_t offset = *(obj->offset);
  struct io_uring_sqe *sqe = 
    static_cast<struct io_uring_sqe *>(p_sqe);
  
  io_uring_prep_writev(sqe,fd,buffer,size,offset);
  io_uring_sqe_set_data(sqe,(void *)&obj->op_state);

return true;
};

#else

/* BELOW CODE FOR NON-IO URING */

bool buffio::OpenFileAwaiter::action(std::pair<void*,void*> info){

  
  auto[p_sqe,p_self] = info;
  buffio::OpenFileAwaiter *obj =
           static_cast<buffio::OpenFileAwaiter *>(p_self);

  int fd = open(obj->path,obj->flags,(mode_t)obj->mode); 
  obj->op_state.op_done = fd;

  return true;
};

bool buffio::ReadFileAwaiter::action(std::pair<void *,void*> info){

 auto[p_sqe,p_self] = info;
 buffio::ReadFileAwaiter *obj =
               static_cast<buffio::ReadFileAwaiter *>(p_self);

 auto[fd,buffer,size,offset] = obj->state;

 ssize_t rval = read(fd,buffer,size);
 obj->op_state.op_done = rval;
 obj->op_state.data = static_cast<void *>(&obj->op_state);

 return true;
};


bool buffio::WriteFileAwaiter::action(std::pair<void*,void*> info){

  auto[p_sqe,p_self] = info;
  buffio::WriteFileAwaiter *obj =
               static_cast<buffio::WriteFileAwaiter *>(p_self);

  auto[fd,buffer,size,offset] = obj->state;
  ssize_t rval = write(fd,buffer,size);

  obj->op_state.op_done = rval;
  obj->op_state.data = static_cast<void *>(&obj->op_state);


 return true;
};

bool buffio::ReadvFileAwaiter::action(std::pair<void *,void *> info){

  auto[p_sqe,p_self] = info;
  buffio::ReadvFileAwaiter *obj =
               static_cast<buffio::ReadvFileAwaiter *>(p_self);

  auto[buffer,size] = obj->state.get();
  ssize_t rval = readv(obj->fd,buffer,size);


  obj->op_state.op_done = rval;
  obj->op_state.data = static_cast<void *>(&obj->op_state);

  return true;
};

bool buffio::WritevFileAwaiter::action(std::pair<void*,void*> info){
 
 auto[p_sqe,p_self] = info;
 buffio::WritevFileAwaiter *obj =
             static_cast<buffio::WritevFileAwaiter *>(p_self);

 auto[buffer,size] = obj->state.get();
 ssize_t rval = writev(obj->fd,buffer,size);

 obj->op_state.op_done = rval;
 obj->op_state.data = static_cast<void*>(&obj->op_state);

 return true;
};

#endif


