#ifndef BUFFIO_FILE
#define BUFFIO_FILE

#include "buffio/core.hpp"
#include "buffio/defs.hpp"
#include <iostream>
#include <filesystem>

using BF_PATH_PREFIX=std::filesystem::path;

namespace buffio {

struct openDir{};

struct openFile{
  bool await_ready() {return true;}
  void await_suspend(buffio::vTask){};
  BFFDOPT await_resume();

  BF_PATH_PREFIX const &path;
  BFLAG flags;
  BMODE mode;
};

struct readFile {

  bool await_ready() { return false; }
  void await_suspend(buffio::vTask _task);
  BFRWOPT await_resume() const {return rval;};
 
  void action(void *me);
 
  BFILE fd;
  char *buffer;
  ssize_t size;
 
  buffio::vTask task;
  BFRWOPT rval;
  BUFFIO_WIN_INSERT(OVERLAPPED win_over);
};

struct readFilev {
  bool await_ready() { return false; };
  void await_suspend(buffio::vTask _task) {};
  BFRWOPT await_resume() const { return rval; };
 
  void action(void *me);

  BFILE fd;
  fileOpVec *vec;
  size_t count;

  buffio::vTask task;
  BFRWOPT rval; 
  BUFFIO_WIN_INSERT(OVERLAPPED win_over);

};

struct writeFile {
  bool await_ready() { return false; }
  void await_suspend(buffio::vTask _task) {};
  BFRWOPT await_resume() const { return rval; }
  void action(void *me);

  BFILE fd;
  char *bufffer;
  ssize_t size;

  buffio::vTask task;
  BFRWOPT rval; 
  BUFFIO_WIN_INSERT(OVERLAPPED win_over);

};


struct writeFilev {
  bool await_ready() { return false; }
  void await_suspend(buffio::vTask _task) {}
  BFRWOPT await_resume() const { return rval; }
  void action(void *me);

  BFILE fd;
  fileOpVec *vec;
  size_t count;

  buffio::vTask task;
  BFRWOPT rval; 
  BUFFIO_WIN_INSERT(OVERLAPPED win_over);

};

struct closeFile{
  bool await_ready()  { return true; }
  void await_suspend(buffio::vTask) noexcept{};
  void await_resume() const { return; }
  BFILE fd;
};

}; // namespace buffio
#undef BF_FILE_API

#endif
