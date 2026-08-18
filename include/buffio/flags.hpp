#ifndef BUFFIO_FLAGS
#define BUFFIO_FLAGS
#include "buffio/os.hpp"

constexpr int B_RDONLY = BUFFIO_OS_INSERT(O_RDONLY,O_RDONLY,1);
constexpr int B_WRONLY = BUFFIO_OS_INSERT(O_WRONLY,O_WRONLY,2);
constexpr int B_APPEND = BUFFIO_OS_INSERT(O_APPEND,O_APPEND,4);
constexpr int B_CREAT = BUFFIO_OS_INSERT(O_CREAT,O_CREAT,8);
constexpr int B_EXCL  = BUFFIO_OS_INSERT(O_EXCL,O_EXCL,16);

#endif
