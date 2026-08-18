#include "buffio/file.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>


BFFDOPT buffio::openFile::await_resume(){

  int err = 0;
  const char *_path = path.c_str();

  if(flags & (O_CREAT | O_EXCL)){ 
    int error = access(_path, F_OK);
    if(error == 0){
      flags &= ~(O_CREAT | O_EXCL);
      err = EEXIST;
    };
  };

  int fd = ::open(_path, flags, mode);
  if(fd < 0)
     return {{BFD_INVALID},errno};

  return {{fd},err};
};

void buffio::readFile::await_suspend(buffio::vTask _task){};
void buffio::readFile::action(struct buffio::readFile *me){};
void buffio::readFilev::action(struct buffio::readFilev *me){};
void buffio::writeFile::action(struct buffio::writeFile *me){};
void buffio::writeFilev::action(struct buffio::writeFilev *me){};



/*

void buffio_os_read(BFD fd, char buffer[], size_t size,ssize_t &rvalue) {
  rvalue = read(fd.fd, buffer, size);
};

ssize_t buffio_os_write(BFD fd, char buffer[], size_t size) {
  return write(fd.fd, buffer, size);
};

#define POPULATE_VEC_FUNC(basename, bfname, ncount)                            \
  struct iovec basename[BUFFIO_FILE_VEC_MAX];                                  \
  for (int i = 0; i < ncount; i++) {                                           \
    basename[i].iov_base = bfname[i].buffer;                                   \
    basename[i].iov_len = bfname[i].len;                                       \
  };

ssize_t buffio_os_readv(BFD fd, fileOpVec vec[], int count) {
  if (count > BUFFIO_FILE_VEC_MAX)
    return -1;
  POPULATE_VEC_FUNC(readVec, vec, count);

  return readv(fd.fd, readVec, count);
};

ssize_t buffio_os_writev(BFD fd, fileOpVec vec[], int count) {
  if (count > BUFFIO_FILE_VEC_MAX)
    return -1;
  POPULATE_VEC_FUNC(writeVec, vec, count);

  return writev(fd.fd, writeVec, count);
};

int buffio_os_rename(BF_PATH_PREFIX const &oldPath,
                     BF_PATH_PREFIX const &newPath) {
  int error = rename(oldPath.c_str(), newPath.c_str());
  return error;
};

int buffio_os_exists(BF_PATH_PREFIX const &path) {
  int error = access(path.c_str(), f_ok);
  return error;
};


int buffio_os_mkdir(BF_PATH_PREFIX const &path) {
  int error = mkdir(path.c_str(), 0666);
  return error;
};

int buffio_os_creat(BF_PATH_PREFIX const &path, BMODE mode) {
  int error = creat(path.c_str(), mode);
  return error;
};

int buffio_os_list_dir(BF_PATH_PREFIX const &path) { return true; };

int buffio_os_ch_dir(BF_PATH_PREFIX const &path) {
  int error = chdir(path.c_str());
  return error;
};

int buffio_os_link(BF_PATH_PREFIX const &path, BF_PATH_PREFIX const &linkPath,
                   int symbolic) {
  int error = 0;
  if (symbolic) {
    error = symlink(path.c_str(), linkPath.c_str());
    return -1;
  };

  error = link(path.c_str(), linkPath.c_str());
  return -1;
};

int buffio_os_unlink(BF_PATH_PREFIX const &path) {
  int error = unlink(path.c_str());
  return error;
};

int buffio_os_close(BFD fd) {
  if (fd.fd < 0)
    return false;
  int error = close(fd.fd);
  return error;
};

*/
