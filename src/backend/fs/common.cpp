#include "buffio/fs.hpp"

int buffio::OpenFileAwaiter::await_resume(){ 
  if(op_state.fd == BUFFIO_FD_INVALID) return -1;

  buffio::File *file = 
       static_cast<buffio::File*>(rval); 

  file->fd = op_state.fd;
  file->flags = 0;
  return 0;
};
