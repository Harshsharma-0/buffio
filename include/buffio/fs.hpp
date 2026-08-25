#ifndef BUFFIO_FS_HPP
#define BUFFIO_FS_HPP

#include "buffio/fsawaitable.hpp"

namespace buffio {

/*
class Fs {
public:
 inline FsMkDirAwaitable MkDir(const char *path,bool async = false) const{
   return FsMkDirAwaitable{(char *)path,async};
 };

 inline FsMkDirAwaitable MkDirAt()const{
  return{};
 };
 

};
*/
class File {
public:
  bool OpenStdIn();
  bool OpenStdOut();

  inline OpenFileAwaiter Open(const char *path, int flags, int mode) const {
    return OpenFileAwaiter{(char *)path, flags, mode, (void *)this};
  };

  inline OpenFileAwaiter Open(BF_PATH_PREFIX &path, int flags, int mode) const {
    return OpenFileAwaiter{(char *)path.c_str(), flags, mode, (void *)this};
  };

  inline ReadFileAwaiter Read(char *buffer, uint32_t size) const {
    return ReadFileAwaiter{{this->fd, buffer, size, (size_t *)&loffset}};
  };

  inline WriteFileAwaiter Write(char *buffer, uint32_t size) const {
    return WriteFileAwaiter{{this->fd, buffer, size, (size_t *)&woffset}};
  };

  inline ReadvFileAwaiter Readv(BuffervState &iovec) const {
    return ReadvFileAwaiter{this->fd, iovec, (size_t *)&loffset};
  };

  inline WritevFileAwaiter Writev(BuffervState &iovec) const {
    return WritevFileAwaiter{this->fd, iovec, (size_t *)&woffset};
  };

  friend struct OpenFileAwaiter;

private:
  buffio_fd fd = BUFFIO_FD_INVALID;
  size_t loffset = 0;
  size_t woffset = 0;
  int flags = 0;
};

}; // namespace buffio

#endif
