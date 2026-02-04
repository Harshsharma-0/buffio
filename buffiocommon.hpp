#ifndef BUFFIO_COMMON
#define BUFFIO_COMMON

#include <coroutine>
#include <sys/socket.h> // for socklen;

#include <cassert>

#define BUFFIO_READ_READY 1
#define BUFFIO_WRITE_READY (1 << 1)
#define BUFFIO_FD_POLLED (1 << 2)
#define BUFFIO_FD_NON_BLOCKING (1 << 3)
#define BUFFIO_FD_ORIGIN_POOL (1 << 4)
#define BUFFIO_FD_REQUEST_AVAILABLE (1 << 5)
#define BUFFIO_FD_CONNECT_REQUEST (1 << 6)
#define BUFFIO_FD_ACCEPT_REQUEST (1 << 7)
#define BUFFIO_FD_READ_REQUEST (1 << 7)
#define BUFFIO_FD_WRITE_REQUEST (1 << 8)

class buffioSockBroker;
class buffioScheduler;
class buffioFdPool;
class buffioThread;
class buffioClock;
class buffioFd;

struct buffioAwaiter;
struct buffioHeader;
template <typename T> struct buffioPromise;


using buffioPromiseHandle = std::coroutine_handle<>;

typedef buffioPromise<int> (*asyncAccept_local)(int fd, struct sockaddr_un, socklen_t );
typedef buffioPromise<int> (*asyncAccept_in)(int fd, struct sockaddr_in , socklen_t );
typedef buffioPromise<int> (*asyncAccept_in6)(int fd, struct sockaddr_in6, socklen_t );

typedef buffioPromise<int> (*asyncConnect)(int errorCode, buffioFd *fd,
                                                   struct sockaddr *);
typedef buffioPromise<int> (*asyncWrite)(int errorCode, char *buffer,
                                                 size_t len, buffioFd *fd,buffioHeader *);
typedef buffioPromise<int> (*asyncRead)(int errorCode, char *buffer,
                                                size_t len, buffioFd *fd,buffioHeader *);

typedef struct buffioHeader {

  /*
   * opCode: it define what kind of operation we want to done on the fd,
   * refer to the "buffioenum.hpp" buffioOpCode enum class for the list of
   * supported opcode.
   *
   */
  buffioOpCode opCode;
  /*
   * rwtype: it defines the function to used to read from the fd,
   * see, buffioenum.hpp for more info.
   *
   */
  buffioReadWriteType rwtype;
  /*
   * relayId: is a unique id for the handler which will handle the header
   * after the request is being done. relay can be customised via the buffio
   * relay class.
   */
  uint16_t relayId;
  /*
   * reqToken is used to synchronize the read and write to the fd,
   * if the request is relayed to the worker thread.
   * read and write operation is gurenteed to be sequential,
   * means the read and write order is maintained under the hood the
   * request that comes first is served first.
   *
   * reqToken is also used as fd field when, the opCode is "poll",
   * the the request token must be the fd for pollling,
   *
   */
  union {
    uint32_t token;
    int mask;
    int fd;
  } reqToken; // To sync the read and write operation;
  /*
   * fd, is the pointer to the fd class created by the user/requested from
   * the eventloop and all the request are done via this class and
   * header is pushed to the worker thread or to the main event loop
   * if using no worker thread.
   */
  buffioFd *fd;
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

  ssize_t reserved;
  char *bufferCursor;
  int unsetBit;
  /*
   * routine contain the handle of the routine to run after
   * the operation is competed. if not an async request
   */
  buffioPromiseHandle routine;

  /*
   * routine used for async requests
   * it pointer to the routine and have a strict return
   * type. see above for the function prototype defination
   */

  union {
    asyncConnect onAsyncConnect;
    asyncRead    onAsyncRead;
    asyncWrite   onAsyncWrite;
    asyncAccept_local    asyncAcceptlocal;
    asyncAccept_in       asyncAcceptin;
    asyncAccept_in6      asyncAcceptin6;
  } onAsyncDone;

  struct buffioHeader *next;
  struct buffioHeader *prev;
} buffioHeader;

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
