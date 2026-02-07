#ifndef BUFFIO_SOCK
#define BUFFIO_SOCK

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cassert>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <type_traits>

#include "common.hpp"
#include "memory.hpp"
#include "sockbroker.hpp"
#include <atomic>
#include <memory>

enum class buffioFdFamily : int {
  none = 0,
  local = 1,
  ipv4 = 2,
  ipv6 = 3,
  raw = 4,
  pipe = 5,
  fifo = 6,
  file = 7,
};

enum class buffioSocketProtocol : int { none = 0, tcp = 1, udp = 2 };

/**
 * @file buffiofd.hpp
 * @author Harsh Sharma
 * @brief  Core fd maker of buffio
 * buffioMakeFd is a wrapper to create fd of any time.
 *
 */

/**
 * @class buffioMakeFd
 * @brief Fd maker for buffio.
 *
 * @details buffioMakeFd is a helper wrapper to create socket, pipe,fifo or
 * opening files easily, without dealing with low level stuff.
 *
 * Supported type of FD in buffioMakeFd
 * - Network socket
 *   - local
 *   - ipv4
 *   - ipv6
 * - pipe
 * - fifo
 *
 * @todo
 *  1. To add supoport to opening a regular file
 *  2. To add protocol string fromat for opening
 */

namespace buffio {

class MakeFd {
public:
  /**
   * @brief create network socket and puts the fd in the buffioFd instance
   *
   * createSocket function provide an easy way to create sockets on linux
   *
   * @param[out] fdCode pointer to the instance of the buffioFd
   * @param[out] lsocket pointer to struct sockaddr_(un/in/in6) based on the
   * family of socket used
   * @param[in] address address of the socket to bind it to
   * @param[in] portNumber portnumber to bind the socket to, ignored for local
   * socket
   * @param[in] family family of socket to create, see buffioFdFamily for list
   * of supported family
   * @param[in] protocol protocol for the socket, see buffioSocketProtocol for
   * list of supported protocol
   * @param[in] blocking set's the socket to blocking or non-blocking based on
   * boolean value, if true socket is blocking
   *
   * @return returns buffioErrorCode, and any value below 0 must be treated as
   * error and taken proper action on Success return buffioErrorCode::none that
   * is 0.
   * @note The return value must not be discarded
   * @todo add generic type fd support, to suport returning fd via int not
   * buffioFd so this class can be used by any program
   *
   */
  [[nodiscard]]
  static int socket(buffio::Fd &fdCore, struct sockaddr *lsocket,
                    const char *address, int portNumber = 8080,
                    buffioFdFamily family = buffioFdFamily::ipv4,
                    buffioSocketProtocol protocol = buffioSocketProtocol::tcp,
                    bool blocking = false);
  /**
   * @brief pipe communication channel maker function
   *
   * @param[in] fdCode pointer to instance of buffioFd
   * @param[in] blocking set's the socket to blocking and non-blocking mode
   * based on boolean value
   *
   * @return returns buffioErrorCode::none on Success, onError return errorcode
   * below 0
   * @note return value by this must not be ignored
   *
   */
  [[nodiscard]]
  static int pipe(buffio::Fd &fdCore, bool blocking = false);

  /**
   * @brief create a fifo communication channel
   *
   * @param[out] pointer to instance of buffiofd
   * @param[in] path path of the fifo, default: /usr/home/buffioDefault
   * @param[in] mode mode for the fifo, default: 0666
   * @param[in] onlyFifo constrol whether to put fifo data in fdCore or not
   *
   * @return returns buffioErrorCode::none, and on error value below 0
   * @note return value by this function must not be discarded
   *
   */
  [[nodiscard]]
  static int mkfifo(buffio::Fd &fdCore,
                    const char *path = "/usr/home/buffioDefault",
                    mode_t mode = 0666, bool onlyFifo = false);
  /**
   * @brief set a fd to non-blocking mode from blocking mode
   *
   * @param[in] fd the fd to change mode
   * @return return buffioErrorCode::none,on Error value below 0
   * @note return value must not be discarded
   *
   */
  static int setNonBlocking(int fd);
};

/**
 * @class buffioFd
 * @brief Fd wrapper for buffio
 *
 * @details buffioFd class provides a wrapper to manage the life cycle of the fd
 * @note Before using these operation fd must be polled to get the defined
 * behaviour:
 *  - waitRead/Write
 *  - asyncRead/write
 *  - asyncAccept/Connect
 *  - waitAccept/Connect
 *
 */

class Fd {

public:
  /**
   * @brief constructs the instance with default value
   */

  Fd() {
    reserveHeader.opCode = buffioOpCode::none;
    localfd = {0};
  };

  /**
   * @brief destructor of buffio fd
   */
  ~Fd();

  /**
   * @brief operator oveload to set a specific bit in read/write mask
   *
   * @return void
   */

  void operator|(int bit) { rwmask |= bit; }

  /**
   * @brief operator overload to check if the bit field
   * is set or not.
   *
   * @return True it specific bit is set, else return false
   *
   */
  bool operator==(int bit) const { return (rwmask & bit); };

  /**
   * @brief method to set a specific bit in read/write mask
   *
   * @return void
   */
  void bitSet(int bit) noexcept { rwmask |= bit; };

  /**
   * @brief method to check if specific bit is set or not in read/write mask
   *
   * @return true if bit is set, else false it not
   *
   */

  bool isBitSet(int bit) const { return (rwmask & bit); };

  /**
   * @brief method to unset bits in read/write mask
   * @return void
   */
  void unsetBit(int bit) noexcept { rwmask &= ~(bit); };

  /**
   * @brief method to get fd, managed by the instance
   *
   * @return fd managed by the instance
   *
   * @note don't use this method to get pipe fd
   */
  int getFd() const { return localfd.fd[0]; };

  /**
   * @brief method returns the reserve header of the fd instance
   *
   * reserve header is internally used by buffioFd, for request other than
   * read/write
   *
   * @return pointer reserveHeader field of the the instance
   */

  const buffioHeader *getReserveHeader() const { return &reserveHeader; };

  /**
   * @brief method to start listening on a socket, owned by the instance
   *
   * @param[in] backlog number of backlog connection to keep
   *
   * @return value returned by the listen system call of linux
   */
  int listen(int backlog) const { return ::listen(localfd.fd[0], backlog); };

  /**
   * @brief method to accept connection on  socket, owned by the instance
   *
   * @param[in] addr pointer to sockaddr_(un/in/in6) to store the information of
   * the accepted connection
   * @param[in] socklen pointer of the length of sockaddr_(un/in/in6)
   *
   * @return a valid fd reffering to the accepted connecton, and -1 if error
   * occurs
   *
   * @note can pass NULL, if don't want to get the address information of the
   * accepted socket
   */
  int accept(struct sockaddr *addr, socklen_t *socklen) const {
    return ::accept(localfd.fd[0], addr, socklen);
  };

  /**
   * @brief method to connect to a socket,
   *
   * @param[in] addr pointer to struct sockaddr_(un/in/in6) if socket is not
   * binded, or NULL
   * @param[in] socklen length of the struct sockaddr_(un/in/in6) if addr is
   * provided, or can leave 0 or NULL
   *
   * @warning you must create the socket with the right address and portnumber
   * you want to connect to, or provide addr and socklen to bind the socket to
   * that address and port if the socket is not binded
   *
   * @return 0 if success, and -1 on error
   *
   */
  int connect(struct sockaddr *addr, socklen_t socklen) const {
    return ::connect(localfd.fd[0], addr, socklen);
  };

  /**
   * @brief method to mark read requests done
   * @param[in] header pointer to valid header that is obtained from the
   * instance
   * @return void
   *
   */
  void asyncReadDone(buffioHeader *header) noexcept;
  /**
   * @brief method to mark write request done
   * @param[in] header pointer to valid header that is obtained from the
   * instance
   * @return void
   *
   */
  void asyncWriteDone(buffioHeader *header) noexcept;
  /**
   * @brief method to asyn accept the connection
   *
   * asyncAccept() is used to accept connection on a socket in async manner
   * without blocking the function.
   *
   * when using async accept reserveHeader cannot be used further
   * @param[in] then valid buffioPromsieHandle to a handler to the socket after
   * acception connection
   * @return void
   *
   * @note before asyncAccept, the user must call the poll method to register
   * the socket for polling, and async accept must be called with buffioamwait
   * ```cpp
   *
   *  buffioPromsie<int> function(){
   *  buffioFd *fd = nullptr;
   *  buffiowait &fd; // get a fd instance from pool
   *  ... setup the fd
   *  buffioawait fd->poll();
   *  //then
   *  buffiowait fd->asyncAccept(handleToTheRoutine);
   *
   * //  do other works
   *  buffioreturn 0;
   *  };
   * ```
   *
   */
  template <typename T> buffioRoutineStatus asyncAccept(T then) {
    if constexpr (std::is_same_v<T, asyncAccept_local>) {
      reserveHeader.opCode = buffioOpCode::asyncAcceptlocal;
      reserveHeader.onAsyncDone.asyncAcceptlocal = then;
    } else if constexpr (std::is_same_v<T, asyncAccept_in>) {
      reserveHeader.opCode = buffioOpCode::asyncAcceptin;
      reserveHeader.onAsyncDone.asyncAcceptin = then;
    } else if constexpr (std::is_same_v<T, asyncAccept_in6>) {
      reserveHeader.opCode = buffioOpCode::asyncAcceptin6;
      reserveHeader.onAsyncDone.asyncAcceptin6 = then;
    } else {
      static_assert(false, "currently we don't support this type of async "
                           "accept function prototype,"
                           "please provide a valid routine, with right "
                           "signature for a specific connection"
                           "type accept request");
    };
     if(!(rwmask & BUFFIO_FD_ACCEPT_READY)){
      this->poll(EPOLLIN);
    };

    reserveHeader.reqToken.fd = localfd.fd[0];
    auto tmp = reserveToQueue(BUFFIO_READ_READY);
    buffio::fiber::pendingReq.fetch_add(1,std::memory_order_acq_rel);

    return buffioRoutineStatus::none;

  };
  /**
   *@brief method to add fd to polling
   *
   * @param[in] mask mask of events to watchout on the fd
   *
   * @return buffioHeader Crafter for poll op
   */

  buffioRoutineStatus poll(int mask = EPOLLIN | EPOLLOUT);

  buffioHeader *getRawHeader() const;
  /**
   * @brief method to async connect to a socket
   *
   * @param then buffio::promiseHandle to push to execution after connecting
   * @note before connection the fd must be polled
   * ```cpp
   *  buffiofd *fd = nullptr
   *  buffiowait &fd; // returns immediately
   *  //setup fd in routine
   *  //create the socket with the address and port to connect
   *  //then
   *  buffiowait fd->asyncConnect(routine); //returns immediately
   * // do other works
   *
   * ```
   * @return buffioHeader pointer that is processed by the promise object
   *
   */

  buffioRoutineStatus asyncConnect(onAsyncConnects then);

  /**
   * @brief method to wait until there any connecton to accept
   *
   * @param[in] addr pointer to sockaddr_(un/in/in6) to get the address of the
   * accepted connection
   * @parma[in] socklen lenght of the respective sockaddr
   * @warning lifecycle of the sockaddr must be valid throughout the entire
   * process of accept
   * @note the poll method must be called before calling waitAccept
   *
   * @return pointer to buffioHeader
   *
   */
  buffioHeader *waitAccept(struct sockaddr *addr, socklen_t socklen);
  /**
   * @brief method to wait until a connection is established to the socket
   *
   * @note the poll method must be called before calling waitConect
   *
   * @return pointer to buffioHeader
   *
   */
  buffioHeader *waitConnect();

  /**
   * @brief linux read call wrapper to read from the fd
   * @param[in] buffer pointer to the buffer to read
   * @param[in] len size of the data in bytes to read
   *
   * @return sizeof the buffer read, or -1 on error
   *
   */

  ssize_t read(char *buffer, size_t len) const {
    return ::read(localfd.fd[0], buffer, len);
  };
  /**
   * @brief linux write call wrapper to write to the fd
   *
   * @param[in]  pointer to the buffer to write
   * @param[in]  len length of the buffer to write
   * @return sizeof the buffer written, or -1 on error
   */
  ssize_t write(char *buffer, size_t len) const {
    return ::write(localfd.fd[0], buffer, len);
  };

  /**
   * @brief method to wait until there data to read
   *
   * @param[in] buffer pointer to the buffer to read
   * @param[in] len size of the data in bytes to read
   *
   * @return buffioHeader crafted for wait read operation
   */

  buffioHeader *waitRead(char *buffer, size_t len);

  /**
   * @brief method to wait until there data to write
   *
   * @param[in] buffer pointer to the buffer to write
   * @param[in] len size of the data in bytes to write
   *
   * @return buffioHeader crafted for wait write operation
   */

  buffioHeader *waitWrite(char *buffer, size_t len);
  /*
   *
   * asyncRead/asyncWrite behaves same as the waitRead/waitWrite, the difference
   * is that asyncRead/asyncWrite, let you push task/routine handle that should
   * be run after, the operation is done.
   * The Header field must be allocated, as it must persist for the life time of
   * the fd. or you can use buffioCall to get a header memory from the memory
   * pool.
   */
  /**
   * @brief async Read from a fd
   * @param[in] header pointer to valid buffioHeader that lifecycle must be
   * valid for async read op
   * @param[in] buffer pointer to the buffer to read
   * @param[in] len size of the data in bytes to read
   *
   * @return buffioHeader crafted for asyncRead
   */

  buffioRoutineStatus asyncRead(char *buffer, size_t len, onAsyncReads then);
  /**
   * @brief async Write to a fd
   * @param[in] header pointer to valid buffioHeader that lifecycle must be
   * valid for async write op
   * @param[in] buffer pointer to the buffer to write
   * @param[in] len size of the data in bytes to write
   *
   * @return buffioHeader crafted for asyncRead
   */

  buffioRoutineStatus asyncWrite(char *buffer, size_t len, onAsyncWrites then);

  /**
   * @brief method to return the read end of a pipe if fd type is pipe
   * @return read fd of pipe
   */
  int getPipeRead() const { return localfd.pipeFd[0]; }

  /**
   * @brief method to return the write end of the pipe
   * @return write fd of pipe
   *
   */
  int getPipeWrite() const { return localfd.pipeFd[1]; }

  /**
   * @brief method to properly close the fd
   * @return void
   */
  void release();

  /*
   * method to enqueue read ad write request in case the fd is not ready to
   * write.
   */

  buffioHeader *getPendingRead() const { return pendingReadReq; }
  buffioHeader *getPendingWrite() const { return pendingWriteReq; }
  buffioHeader *reserveToQueue(int rmBit);
  void resetHeader(){
    ::memset((void*)&reserveHeader,'\0',sizeof(buffioHeader));
  };
  void popPendingWrite() noexcept { pendingWriteReq = nullptr; };
  void popPendingRead() noexcept { pendingReadReq = nullptr; };

  friend class buffio::MakeFd;

private:
  /**
   * @brief method to mount the socket created
   * @param[in] address pointer to the address string allocated using new
   * @param[in] socketFd socketfd created by user
   * @param[in] portnumber portnumber of the socket
   * @note addredd is only used for UNIX socket
   * @return void
   */

  void mountSocket(char *address, int socketfd, int portnumber) noexcept;
  /*@brief method to mount the pipe
   * @param[in] read read end of the pipe
   * @param[in] write write end of the pipe
   * @return void
   */
  void mountPipe(int read, int write) noexcept;

  /**
   * @brief method to mount the fifo
   * @param[in] address address of the fifo, must be a valid string allocated
   * using new
   * @return void
   */

  void mountFifo(char *address) { this->address = address; };

  buffioFdFamily fdFamily = buffioFdFamily::none;
  buffioOrigin origin = buffioOrigin::routine;

protected:
  /**
   * @brief rwmask define the readyness of the file descriptor.
   * see buffiocommon.hpp to see the mask defined for readyness
   * of the fd,
   *
   * @note
   *  - BUFFIO_READ_READY: if masked with this, it means the fd is
   *          ready for read operation.
   *
   *  - BUFFIO_WRITE_READY: if masked with this, it means the fd is
   *          ready for write operation.
   *
   */

  int rwmask = 0;

  /**
   * @brief pointer to address of the fifo or unix socketpath
   * @note pointer to the address must be allocated with new operator
   */
  char *address = nullptr;

  /**
   * @brief union to hold the respective fd based on family type
   */
  union localFdInfo {
    struct socketInfo {
      int socketFd;   ///< socket fd
      int portnumber; ///< socket portnumber
    } sock;           ///< field to store socket information
    int fileFd;       ///< field to store filefd
    int pipeFd[2];    ///< field to store pipe fds
    int fd[2];        ///< field for generic access to the fd
  } localfd;

  /**
   *@brief field used for some internal works for the fd request,
   * request other that read/write relay on reserveHeader
   *
   * reserveHeader is only used for asyncAccept/Connect
   * or waitAccept/waitConnect
   */
  buffioHeader reserveHeader;
  buffioHeader *pendingReadReq = nullptr;
  buffioHeader *pendingWriteReq = nullptr;
};
}; // namespace buffio

#endif
