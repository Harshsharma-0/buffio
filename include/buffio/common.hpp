#ifndef BUFFIO_COMMON
#define BUFFIO_COMMON

#include "enum.hpp"
#include <cassert>
#include <coroutine>
#include <cstdint>
#include <sys/socket.h> // for socklen;

#define BUFFIO_READ_READY 1
#define BUFFIO_WRITE_READY (1 << 1)
#define BUFFIO_FD_POLLED (1 << 2)
#define BUFFIO_FD_NON_BLOCKING (1 << 3)
#define BUFFIO_FD_ORIGIN_POOL (1 << 4)
#define BUFFIO_FD_REQUEST_AVAILABLE (1 << 5)
#define BUFFIO_FD_CONNECT_REQUEST (1 << 6)
#define BUFFIO_FD_ACCEPT_READY (1 << 7)
#define BUFFIO_FD_READ_REQUEST (1 << 8)
#define BUFFIO_FD_WRITE_REQUEST (1 << 9)

#define BUFFIO_HEADER_NEWED -11111111
namespace buffio {
class Fd;
class sockBroker;
template <typename T> struct promise;
using promiseHandle = std::coroutine_handle<>;

}; // namespace buffio
struct buffioHeader;

using promiseHandle = std::coroutine_handle<>;

typedef buffio::promise<int> (*asyncAccept_local)(int fd, struct sockaddr_un,
                                                  socklen_t);
typedef buffio::promise<int> (*asyncAccept_in)(int fd, struct sockaddr_in,
                                               socklen_t);
typedef buffio::promise<int> (*asyncAccept_in6)(int fd, struct sockaddr_in6,
                                                socklen_t);

typedef buffio::promise<int> (*onAsyncConnects)(int errorCode, buffio::Fd *fd,
                                                struct sockaddr *);
typedef buffio::promise<int> (*onAsyncWrites)(int errorCode, char *buffer,
                                              size_t len, buffio::Fd *fd);
typedef buffio::promise<int> (*onAsyncReads)(int errorCode, char *buffer,
                                             size_t len, buffio::Fd *fd);
typedef buffio::promiseHandle (*buffioAction)(buffioHeader *);

typedef struct buffioHeader {

  bool isFresh;
  int iFd;
  /*
   * fd, is the pointer to the fd class created by the user/requested from
   * the eventloop and all the request are done via this class and
   * header is pushed to the worker thread or to the main event loop
   * if using no worker thread.
   */
  buffio::Fd *fd;
  /*
   * buffer field contains buffer in which the user want to read/write
   * the buffer must maintain it's lifecycle, until or unless the operation
   * is completed on buffer.
   */
  union {
    char *buffer;
    struct sockaddr *socketaddr;
  } data;
  /*
   * len of buffer, must be equal to the size of the buffer, and the amount
   * of data read is returned by this field.
   */
  union {
    ssize_t len;
    socklen_t socklen;
    int mask;
  } len;
  /*
   * reserved field is used for to count the number of bytes that is
   * read/write form the fd, in total.
   *
   */

  int opError;
  int aux;
  /*
   * routine contain the handle of the routine to run after
   * the operation is competed. if not an async request
   */
  buffio::promiseHandle routine;
  buffio::promiseHandle then;

  /*
   * routine used for async requests
   * it pointer to the routine and have a strict return
   * type. see above for the function prototype defination
   */

  union {
    onAsyncConnects onAsyncConnect;
    onAsyncReads onAsyncRead;
    onAsyncWrites onAsyncWrite;
    asyncAccept_local asyncAcceptlocal;
    asyncAccept_in asyncAcceptin;
    asyncAccept_in6 asyncAcceptin6;
   } onAsyncDone;

  buffioAction action;

  struct buffioHeader *next;
  struct buffioHeader *prev;
} buffioHeader;

typedef struct buffioHeaderSync {
  buffioHeader *headerA;
  buffioHeader *headerB;
} buffioHeaderSync;

/* supported protocol string format in future update:
 * tcp - "tcp://127.0.0.1:8080".
 * udp - "udp://127.0.0.1:8081".
 * file - "file://path:usr/mytype.txt".
 * fifo - "fifo://path:/home/usr/fifo".
 * pipe - "pipe://path:null".
 */

typedef struct buffioTimer {
  uint64_t duration;
  int repeat;
  std::coroutine_handle<> then;
} buffioTimer;

using buffioHeaderType = buffioHeader;

#endif
