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

#if defined(BUFFIO_OS_WINDOWS)
  inline OpenFileAwaiter Open(const wchar_t *path, int flags, int mode) {
    OpenFileAwaiter awaiter;
    awaiter.path = path;
    awaiter.flags = flags;
    awaiter.mode = mode;
    awaiter.rval = (void *)this;
    return awaiter;
  };
  
  inline OpenFileAwaiter Open(BF_PATH_PREFIX &path, int flags, int mode) const {
    return OpenFileAwaiter{path.wstring(), flags, mode, (void *)this};
  };

#elif defined(BUFFIO_OS_LINUX)


  inline OpenFileAwaiter Open(const char *path, int flags, int mode) const {
    return OpenFileAwaiter{(char *)path, flags, mode, (void *)this};
  };
  
  inline OpenFileAwaiter Open(BF_PATH_PREFIX &path, int flags, int mode) const {
    return OpenFileAwaiter{(char *)path.c_str(), flags, mode, (void *)this};
  };

#endif

#if defined(BUFFIO_BACKEND_EPOLL) || defined(BUFFIO_BACKEND_IOCP)

  inline ReadFileAwaiter Read(char *buffer, size_t size) const {
    return ReadFileAwaiter{this->fd, buffer, size};
  };

  inline WriteFileAwaiter Write(char *buffer, size_t size) const {
    return WriteFileAwaiter{this->fd, buffer, size};
  };

  inline ReadvFileAwaiter Readv(BuffervState &iovec) const {
    auto [buffer, size] = iovec.get();
    return ReadvFileAwaiter{this->fd, (char *)buffer, size};
  };

  inline WritevFileAwaiter Writev(BuffervState &iovec) const {
    auto [buffer, size] = iovec.get();
    return WritevFileAwaiter{this->fd, (char *)buffer, size};
  };

#elifdef BUFFIO_BACKEND_IOURING

  inline ReadFileAwaiter Read(char *buffer, size_t size){
    return ReadFileAwaiter{this->fd, buffer, size, roffset,
                           &roffset};
  };

  inline WriteFileAwaiter Write(char *buffer, size_t size){
    return WriteFileAwaiter{this->fd, buffer, size, woffset,
                            &woffset};
  };

  inline ReadvFileAwaiter Readv(BuffervState &iovec){
    auto [buffer, size] = iovec.get();
    return ReadvFileAwaiter{this->fd, (char *)buffer, size, roffset,
                            &roffset};
  };

  inline WritevFileAwaiter Writev(BuffervState &iovec){
    auto [buffer, size] = iovec.get();
    return WritevFileAwaiter{this->fd, (char *)buffer, size, woffset,
                             &woffset};
  };

#endif

  inline ReadOffsetFileAwaiter ReadAt(char *buffer, size_t size,
                                       uint64_t off) const {
    return ReadOffsetFileAwaiter{this->fd, buffer, size, off};
  };

  inline WriteOffsetFileAwaiter WriteAt(char *buffer, size_t size,
                                         uint64_t off) const {
    return WriteOffsetFileAwaiter{this->fd, buffer, size, off};
  };

  inline ReadvOffsetFileAwaiter ReadvAt(BuffervState &iovec,
                                         uint64_t off) const {
    auto [buffer, size] = iovec.get();
    return ReadvOffsetFileAwaiter{this->fd, (char *)buffer, size, off};
  };
  inline WritevOffsetAwaitable WritevAt(BuffervState &iovec,
                                         uint64_t off) const {
    auto [buffer, size] = iovec.get();
    return WritevOffsetAwaitable{this->fd, (char *)buffer, size, off};
  };

  friend struct OpenFileAwaiter;

private:
  buffio_fd fd = BUFFIO_FD_INVALID;
#ifdef BUFFIO_BACKEND_IOURING
  uint64_t roffset = 0;
  uint64_t woffset = 0;
#endif
  int flags = 0;
};

}; // namespace buffio

#endif
