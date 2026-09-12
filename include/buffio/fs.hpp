#ifndef BUFFIO_FS_HPP
#define BUFFIO_FS_HPP

#include "buffio/fsawaitable.hpp"


#if defined(BUFFIO_OS_WINDOWS)
#define BUFFIO_WIDEN_HELPER(str) L##str
#else
#define BUFFIO_WIDEN_HELPER(str) str
#endif

#define BUFFIO_WIDEN(name) BUFFIO_WIDEN_HELPER(name)

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

class Path:public std::filesystem::path{};

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
  
  inline OpenFileAwaiter Open(buffioPath &path, int flags, int mode) const {
    return OpenFileAwaiter{path.wstring(), flags, mode, (void *)this};
  };

#elif defined(BUFFIO_OS_LINUX)


  inline OpenFileAwaiter Open(const char *path, int flags, int mode) const {
    return OpenFileAwaiter{(char *)path, flags, mode, (void *)this};
  };
  
  inline OpenFileAwaiter Open(buffio::Path &path, int flags, int mode) const {
    return OpenFileAwaiter{(char *)path.c_str(), flags, mode, (void *)this};
  };

#endif

#if defined(BUFFIO_BACKEND_EPOLL) || defined(BUFFIO_BACKEND_IOCP)

 /*========================================================
  *
  * function defination/overload for read operation 
  *
  *=========================================================
  */

  inline ReadFileAwaiter Read(char *buffer, size_t size) const {
    return ReadFileAwaiter{this->fd, buffer, size};
  };
 
  inline ReadvFileAwaiter Readv(BuffervState &iovec) const {
    auto [buffer, size] = iovec.get();
    return ReadvFileAwaiter{this->fd, (char *)buffer, size};
  };

  inline ReadvFileAwaiter Readv(Bufferiov &vec,int count) const {
    return ReadvFileAwaiter{this->fd, (char *)&vec, static_cast<size_t>(count)};
  };

  inline ReadvOffsetFileAwaiter ReadvAt(Bufferiov &vec,int count,
                                         uint64_t off) const {
    return ReadvOffsetFileAwaiter{this->fd, (char *)&vec, static_cast<size_t>(count), off};
  };

  inline ReadvOffsetFileAwaiter ReadvAt(BuffervState &iovec,
                                         uint64_t off) const {
    auto[buffer,size] = iovec.get();
    return ReadvOffsetFileAwaiter{this->fd,(char*)buffer, size, off};
  };

  inline ReadOffsetFileAwaiter ReadAt(char *buffer, size_t size,
                                       uint64_t off) const {
    return ReadOffsetFileAwaiter{this->fd, buffer, size, off};
  };


/*=========================================================
 *
 * function defination/overload for write operation 
 *
 *=========================================================
 */

  inline WriteFileAwaiter Write(char *buffer, size_t size) const {
    return WriteFileAwaiter{this->fd, buffer, size};
  };

  inline WritevFileAwaiter Writev(BuffervState &iovec) const {
    auto [buffer, size] = iovec.get();
    return WritevFileAwaiter{this->fd, (char *)buffer, size};
  };

  inline WritevFileAwaiter Writev(Bufferiov &vec,int count) const {
    return WritevFileAwaiter{this->fd, (char *)&vec, static_cast<size_t>(count)};
  };

  inline WriteOffsetFileAwaiter WriteAt(char *buffer, size_t size,
                                         uint64_t off) const {
    return WriteOffsetFileAwaiter{this->fd, buffer, size, off};
  };

 
  inline WritevOffsetAwaitable WritevAt(Bufferiov &vec,int count,
                                         uint64_t off) const {
    return WritevOffsetAwaitable{this->fd, (char *)&vec,static_cast<size_t>(count), off};
  };

  inline WritevOffsetAwaitable WritevAt(BuffervState &iovec,
                                         uint64_t off) const {
    auto[buffer,size] = iovec.get();
    return WritevOffsetAwaitable{this->fd, (char *)buffer,size, off};
  };

#elifdef BUFFIO_BACKEND_IOURING

/*=========================================================
 *
 * function defination/overload for read operation 
 *
 *=========================================================
 */

  inline ReadFileAwaiter Read(char *buffer, size_t size)  {
    return ReadFileAwaiter{this->fd, buffer, size,roffset,&roffset};
  };
 
  inline ReadvFileAwaiter Readv(BuffervState &iovec)  {
    auto [buffer, size] = iovec.get();
    return ReadvFileAwaiter{this->fd, (char *)buffer, size,roffset,&roffset};
  };

  inline ReadvFileAwaiter Readv(Bufferiov &vec,int count)  {
    return ReadvFileAwaiter{this->fd, (char *)&vec, static_cast<size_t>(count),roffset,&roffset};
  };

  inline ReadvOffsetFileAwaiter ReadvAt(Bufferiov &vec,int count,
                                         uint64_t off)  {
    return ReadvOffsetFileAwaiter{this->fd, (char *)&vec, static_cast<size_t>(count), off,nullptr};
  };

  inline ReadvOffsetFileAwaiter ReadvAt(BuffervState &iovec,
                                         uint64_t off)  {
    auto[buffer,size] = iovec.get();
    return ReadvOffsetFileAwaiter{this->fd,(char*)buffer, size, off,nullptr};
  };

  inline ReadOffsetFileAwaiter ReadAt(char *buffer, size_t size,
                                       uint64_t off)  {
    return ReadOffsetFileAwaiter{this->fd, buffer, size, off,nullptr};
  };


/*=========================================================
 *
 * function defination/overload for write operation 
 *
 *=========================================================
 */

  inline WriteFileAwaiter Write(char *buffer, size_t size)  {
    return WriteFileAwaiter{this->fd, buffer, size,woffset,&woffset};
  };

  inline WritevFileAwaiter Writev(BuffervState &iovec)  {
    auto [buffer, size] = iovec.get();
    return WritevFileAwaiter{this->fd, (char *)buffer, size,woffset,&woffset};
  };

  inline WritevFileAwaiter Writev(Bufferiov &vec,int count)  {
    return WritevFileAwaiter{this->fd, (char *)&vec, static_cast<size_t>(count),woffset,&woffset};
  };

  inline WriteOffsetFileAwaiter WriteAt(char *buffer, size_t size,
                                         uint64_t off)  {
    return WriteOffsetFileAwaiter{this->fd, buffer, size, off,nullptr};
  };

 
  inline WritevOffsetAwaitable WritevAt(Bufferiov &vec,int count,
                                         uint64_t off)  {
    return WritevOffsetAwaitable{this->fd, (char *)&vec, static_cast<size_t>(count), off,nullptr};
  };

  inline WritevOffsetAwaitable WritevAt(BuffervState &iovec,
                                         uint64_t off)  {
    auto[buffer,size] = iovec.get();
    return WritevOffsetAwaitable{this->fd, (char *)buffer,size, off,nullptr};
  };


#endif


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
