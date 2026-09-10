#include "buffio/config.hpp"
#include "buffio/fs.hpp"
#include "buffio/worker.hpp"

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
  if(op_state.fd == BUFFIO_FD_INVALID) return -1;

  buffio::File *file = 
       static_cast<buffio::File*>(rval); 

  file->fd = op_state.fd;
  file->loffset = 0;

  return 0;
};
