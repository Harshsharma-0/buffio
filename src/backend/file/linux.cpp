#include "buffio/file.hpp"
#include "buffio/worker.hpp"

void buffio::ReadFileAwaiter::action(void *me){

  auto[fd,buffer,size,offset] = state;
 #ifdef BUFFIO_WORKER_IOURING_HPP

  struct io_uring_sqe *sqe = static_cast<struct io_uring_sqe *>(me);
  
  identifier = (ReadFileAwaiter *)this;
  io_uring_prep_read(sqe,fd,buffer,size,*offset);
  io_uring_sqe_set_data(sqe,(void *)&identifier);
  
 return;
 #endif

  rval = read(fd,buffer,size);
 return;
};

void buffio::ReadvFileAwaiter::action(void *me){};

void buffio::WriteFileAwaiter::action(void *me){
  auto[fd,buffer,size,offset] = state;

 #ifdef BUFFIO_WORKER_IOURING_HPP

  struct io_uring_sqe *sqe = static_cast<struct io_uring_sqe *>(me);
  
  identifier = this;
  io_uring_prep_write(sqe,fd,buffer,size,*offset);
  io_uring_sqe_set_data(sqe,(void *)&identifier);

 return;
 #else

  rval = write(fd,buffer,size);
 #endif
 return;
};

void buffio::WritevFileAwaiter::action(void *me){};
