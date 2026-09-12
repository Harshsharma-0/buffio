#include "buffio/fs.hpp"
#include <unistd.h>
#include <fcntl.h>

int buffio::OpenFileAwaiter::await_resume(){ 
  if(op_state.fd == BUFFIO_FD_INVALID) return op_state.error;

  buffio::File *file = 
       static_cast<buffio::File*>(rval); 

  file->fd = op_state.fd;
  file->flags = 0;
  return 0;
};
bool buffio::File::OpenStdIn(){
  //check if we are not trying to open on fd that is pre occupied
  assert(fd == BUFFIO_FD_INVALID);
  fd = STDIN_FILENO;
  return true; 
};

bool buffio::File::OpenStdOut(){ 
  //check if we are not trying to open on fd that is pre occupied
  assert(fd == BUFFIO_FD_INVALID);
  fd = STDOUT_FILENO;
  return true;
};

