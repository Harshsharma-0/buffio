#ifndef BUFFIO_FS_HPP
#define BUFFIO_FS_HPP

#include "buffio/fsawaitable.hpp"

namespace buffio {

class File {
public:

  bool OpenStdIn();
  bool OpenStdOut();


  inline OpenFileAwaiter Open(buffio::Path &path, int flags, int mode) const {
    return OpenFileAwaiter{path, flags, mode, (void *)this};
  };
  
 /*========================================================
  *
  * function defination/overload for read operation 
  *
  *=========================================================
  */

  inline ReadFileAwaiter Read(char *buffer, size_t size)  {
    return ReadFileAwaiter{this->fd, buffer, size,
          BUFFIO_IOURING_INSERT(roffset,0),
          BUFFIO_IOURING_INSERT(&roffset,nullptr)};
  };

 
  inline ReadvFileAwaiter Readv(BuffervState &iovec)  {
    auto [buffer, size] = iovec.get();

    return ReadvFileAwaiter{this->fd, (char *)buffer, size,
                            BUFFIO_IOURING_INSERT(roffset,0),
                            BUFFIO_IOURING_INSERT(&roffset,nullptr)};

  };


  inline ReadvFileAwaiter Readv(Bufferiov &vec,int count)  {
    return ReadvFileAwaiter{this->fd, (char *)&vec, 
                            static_cast<size_t>(count),
                            BUFFIO_IOURING_INSERT(roffset,0),
                            BUFFIO_IOURING_INSERT(&roffset,nullptr)};
  };

  inline ReadvOffsetFileAwaiter ReadvAt(Bufferiov &vec,int count,
                                         uint64_t off)  {
    return ReadvOffsetFileAwaiter{this->fd,
                                  (char *)&vec, 
                                  static_cast<size_t>(count), 
                                  off,nullptr};

  };

  inline ReadvOffsetFileAwaiter ReadvAt(BuffervState &iovec,
                                         uint64_t off)  {
    auto[buffer,size] = iovec.get();
    return ReadvOffsetFileAwaiter{this->fd,(char*)buffer, size,
                                  off,nullptr};
  };

  inline ReadOffsetFileAwaiter ReadAt(char *buffer, size_t size,
                                       uint64_t off)  {
    return ReadOffsetFileAwaiter{this->fd, buffer, size,
                                 off,nullptr};
  };


  

/*=========================================================
 *
 * function defination/overload for write operation 
 *
 *=========================================================
 */

  inline WriteFileAwaiter Write(char *buffer, size_t size)  {
    return WriteFileAwaiter{this->fd, buffer, size,
                            BUFFIO_IOURING_INSERT(woffset,0),
                            BUFFIO_IOURING_INSERT(&woffset,0)};
  };

  inline WritevFileAwaiter Writev(BuffervState &iovec)  {
    auto [buffer, size] = iovec.get();
    return WritevFileAwaiter{this->fd, (char *)buffer, size,
                             BUFFIO_IOURING_INSERT(woffset,0),
                             BUFFIO_IOURING_INSERT(&woffset,nullptr)};
  };

  inline WritevFileAwaiter Writev(Bufferiov &vec,int count)  {
    return WritevFileAwaiter{this->fd, (char *)&vec, 
                             static_cast<size_t>(count),
                             BUFFIO_IOURING_INSERT(woffset,0),
                             BUFFIO_IOURING_INSERT(&woffset,nullptr)};
  };

  inline WriteOffsetFileAwaiter WriteAt(char *buffer, size_t size,
                                         uint64_t off)  {
    return WriteOffsetFileAwaiter{this->fd, buffer, size, 
                                  off,nullptr};
  };

 
  inline WritevOffsetAwaitable WritevAt(Bufferiov &vec,int count,
                                         uint64_t off)  {
    return WritevOffsetAwaitable{this->fd, (char *)&vec, 
                                static_cast<size_t>(count), 
                                off,nullptr};
  };

  inline WritevOffsetAwaitable WritevAt(BuffervState &iovec,
                                         uint64_t off)  {
    auto[buffer,size] = iovec.get();
    return WritevOffsetAwaitable{this->fd, (char *)buffer,
                                 size, off,nullptr};
  };


  friend struct buffio::OpenFileAwaiter;

private:
  buffio_fd fd = BUFFIO_FD_INVALID;

#ifdef BUFFIO_BACKEND_IOURING
  uint64_t roffset = 0;
  uint64_t woffset = 0;
#endif

  int flags = 0;
};

namespace Fs{
 inline FsMkDirAwaitable MkDir(buffio::Path &path,int mode_t){
   return FsMkDirAwaitable{path,mode_t,path};
 };
 inline FsRenameAwaitable Rename(buffio::Path &oldName,buffio::Path &newName){
   return FsRenameAwaitable{newName,0,oldName};
 };
};
}; // namespace buffio

#endif
