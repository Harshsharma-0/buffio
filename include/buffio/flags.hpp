#ifndef BUFFIO_FLAGS
#define BUFFIO_FLAGS
#include "buffio/config.hpp"


/* open for read-only*/
constexpr int B_RDONLY = BUFFIO_OS_INSERT(O_RDONLY,O_RDONLY,1U);

/* open for write-only */
constexpr int B_WRONLY = BUFFIO_OS_INSERT(O_WRONLY,O_WRONLY,(1U << 1));

/* append data at end of file on every write req */
constexpr int B_APPEND = BUFFIO_OS_INSERT(O_APPEND,O_APPEND,(1U << 2));

/* if the file / dir not exist create */
constexpr int B_CREAT = BUFFIO_OS_INSERT(O_CREAT,O_CREAT,(1U << 3));

/* if specified with B_CREAT, open call will fail if the resource exists */
constexpr int B_EXCL  = BUFFIO_OS_INSERT(O_EXCL,O_EXCL,(1U << 4));

/* open file  both read and write */
constexpr int B_RDWR = BUFFIO_OS_INSERT(O_RDWR,O_RDWR,B_RDONLY | B_WRONLY);

/* close on exec only on linux */
constexpr int B_CLOEXEC = BUFFIO_OS_INSERT(O_CLOEXEC,O_CLOEXEC,5);

/* open the path if it's directory, else fail only on linux*/
constexpr int B_DIRECTORY = BUFFIO_OS_INSERT(O_DIRECTORY,O_DIRECTORY,(1U << 6));

/* if specified and the path is a symbloic-link, it will no be dereffered */
constexpr int B_NOFOLLOW = BUFFIO_OS_INSERT(O_NOFOLLOW,O_NOFOLLOW,(1U << 7));

/* open the file in non-blocking mode */
constexpr int B_NONBLOCK = BUFFIO_OS_INSERT(O_NONBLOCK,O_NONBLOCK,(1U << 8));

/* Discard the contents of file, if specified */
constexpr int B_TRUNC = BUFFIO_OS_INSERT(O_TRUNC,O_TRUNC,(1U << 9));

/* Create a unnamed tmp file, spedified at the directory given in path */
constexpr int B_TMPFILE = BUFFIO_OS_INSERT(O_TMPFILE,O_TMPFILE,(1U << 10));


//TODO: add mode_t/access mode defs to the flags for Open
#endif
