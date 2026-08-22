#ifndef BUFFIO_FILE_HPP
#define BUFFIO_FILE_HPP

#include "buffio/core.hpp"

namespace buffio {

struct ReadFileAwaiter {

  bool await_ready() { return false; }
  void await_suspend(buffio::CoroutineHandle task_);
  ssize_t await_resume() const {return rval;};
 
  void action(void *me);
  
  buffio::ReadFile state;
  buffio::CoroutineHandle task;
  buffio::op_vec identifier;
  Worker *worker;
  ssize_t rval;

  BUFFIO_WIN_INSERT(OVERLAPPED win_over);
};

struct ReadvFileAwaiter {
  bool await_ready() { return false; };
  void await_suspend(buffio::CoroutineHandle task_);
  ssize_t await_resume() const { return rval; };
 
  void action(void *me);
  
  buffio::ReadvFile state;
  buffio::CoroutineHandle task;
  buffio::op_vec identifier;
  Worker* worker;

  ssize_t rval;

  BUFFIO_WIN_INSERT(OVERLAPPED win_over);

};

struct WriteFileAwaiter {
  bool await_ready() { return false; }
  void await_suspend(buffio::CoroutineHandle task_);
  ssize_t await_resume() const { return rval; }
  void action(void *me);

  buffio::WriteFile state;
  buffio::CoroutineHandle task;
  buffio::op_vec identifier;
  Worker* worker;

  ssize_t rval;

  BUFFIO_WIN_INSERT(OVERLAPPED win_over);

};


struct WritevFileAwaiter {

  bool await_ready() { return false; }
  void await_suspend(buffio::CoroutineHandle task_);
  ssize_t await_resume() const { return rval; }
  void action(void *me);

  buffio::WritevFile state;
  buffio::CoroutineHandle task;
  Worker* worker;

  buffio::op_vec identifier;
 
  ssize_t rval;

  BUFFIO_WIN_INSERT(OVERLAPPED win_over);

};

struct CloseFileAwaiter{
  bool await_ready()  { return true; }
  void await_suspend(buffio::CoroutineHandle) noexcept{};
  void await_resume() const { return; }
  BFILE fd;
};


struct File{
  public:

   inline buffio::ReadFileAwaiter read(char *buffer,size_t size) const { 
      return buffio::ReadFileAwaiter{{this->fd,buffer,size,(size_t*)&loffset}}; 
    };

    buffio::ReadvFile readv(buffio::ReadFile *vec,size_t size){
      return buffio::ReadvFile{this->fd,vec,size,&loffset};
    };

    buffio::WriteFile write(char *buffer,size_t size){
      return buffio::WriteFile{this->fd,buffer,size,&loffset};
    };

    buffio::WritevFile writev(buffio::WriteFile *vec,size_t size){
     return buffio::WritevFile{this->fd,vec,size,&loffset};
    };
    
 buffio_fd fd;
private:

 size_t loffset = 0;
 int flags = 0;
};


}; // namespace buffio


#endif
