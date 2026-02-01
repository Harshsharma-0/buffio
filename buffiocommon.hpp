#ifndef BUFFIO_COMMON
#define BUFFIO_COMMON

#include <coroutine>
#include <cassert>

#define BUFFIO_READ_READY 1
#define BUFFIO_WRITE_READY (1 << 1)


class buffioSockBroker;
class buffioScheduler;
class buffioFdPool;
class buffioThread;
class buffioClock;
class buffioFd;

struct buffioAwaiter;
template<typename T>
struct buffiopromise;


using buffioPromiseHandle = std::coroutine_handle<>;

typedef struct buffioHeader {

 /* 
  * opCode: it define what kind of operation we want to done on the fd,
  * refer to the "buffioenum.hpp" buffioOpCode enum class for the list of
  * supported opcode.
  *
  */
  buffioOpCode opCode; 
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
  union{
    uint32_t token;
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
  char *buffer;
  /*
  * len of buffer, must be equal to the size of the buffer, and the amount
  * of data read is returned by this field.
  */
  size_t len;
  /*
   * routine contain the handle of the routine to run after 
   * the operation is competed.
   */
  buffioPromiseHandle routine;

  /*
   * fields to chain the requests on a fd, 
   * if shared by multiple instance.
   */
  buffioHeader *next;
} buffioHeader;

/* supported protocol string format:
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
