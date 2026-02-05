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

class buffioFd {
public:
  /**
   * @brief constructs the instance with default value
   */

  buffioFd() { localfd = {0}; };

  /**
   * @brief destructor of buffio fd
   */
  ~buffioFd() { release(); };

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
   * @brief method to get the read request header
   *
   * @note fd instance support only one subsiquent read/write request at any
   * instance of time if there another request made while the pending one is not
   * done, the request is dropped
   *
   * @return nullptr, if there no request, or valid request header
   */

  buffioHeader *getReadReq() const { return readReq; };

  /**
   * @brief method to get the write request header
   *
   * @note fd instance support only one subsiquent read/write request at any
   * instance of time if there another request made while the pending one is not
   * done, the request is dropped
   *
   * @return nullptr, if there no request, or valid request header
   */

  buffioHeader *getWriteReq() const { return writeReq; };

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
  void asyncReadDone(buffioHeader *header) noexcept {
    if (readReq == header) {
      readReq = header->next;
      return;
    };

    if (header->prev)
      header->prev->next = header->next;
    if (header->next)
      header->next->prev = header->prev;
  };
  /**
   * @brief method to mark write request done
   * @param[in] header pointer to valid header that is obtained from the
   * instance
   * @return void
   *
   */

  void asyncWriteDone(buffioHeader *header) noexcept {
    if (writeReq == header) {
      writeReq = header->next;
      return;
    };

    if (header->prev)
      header->prev->next = header->next;
    if (header->next)
      header->next->prev = header->prev;
  };

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
   * the socket for polling, and async accept must be called with buffioawait
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
  template <typename T> buffioHeader *asyncAccept(T then) {

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
    reserveHeader.reqToken.fd = localfd.fd[0];

    return &reserveHeader;
  };

  /**
   * @brief method to async connect to a socket
   *
   * @param then buffioPromiseHandle to push to execution after connecting
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

  buffioHeader *asyncConnect(asyncConnect then) {
    reserveHeader.opCode = buffioOpCode::asyncConnect;
    reserveHeader.reqToken.fd = localfd.fd[0];
    reserveHeader.onAsyncDone.onAsyncConnect = then;
    return &reserveHeader;
  };

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
  buffioHeader *waitAccept(struct sockaddr *addr, socklen_t socklen) {
    reserveHeader.opCode = buffioOpCode::waitAccept;
    reserveHeader.reqToken.fd = localfd.fd[0];
    reserveHeader.len.socklen = socklen;
    reserveHeader.data.socketaddr = addr;
    return &reserveHeader;
  };

  /**
   * @brief method to wait until a connection is established to the socket
   *
   * @note the poll method must be called before calling waitConect
   *
   * @return pointer to buffioHeader
   *
   */

  buffioHeader *waitConnect() {
    reserveHeader.opCode = buffioOpCode::waitConnect;
    reserveHeader.reqToken.fd = localfd.fd[0];
    return &reserveHeader;
  };

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

  buffioHeader *waitRead(char *buffer, size_t len) {
    reserveHeader.opCode = buffioOpCode::read;
    reserveHeader.reqToken.fd = localfd.fd[0];
    reserveHeader.len.len = len;
    reserveHeader.data.buffer = buffer;
    reserveHeader.bufferCursor = buffer;
    reserveHeader.reserved = 0;
    return &reserveHeader;
  };

  /**
   * @brief method to wait until there data to write
   *
   * @param[in] buffer pointer to the buffer to write
   * @param[in] len size of the data in bytes to write
   *
   * @return buffioHeader crafted for wait write operation
   */

  buffioHeader *waitWrite(char *buffer, size_t len) {
    reserveHeader.opCode = buffioOpCode::write;
    reserveHeader.reqToken.fd = localfd.fd[0];
    reserveHeader.len.len = len;
    reserveHeader.data.buffer = buffer;
    reserveHeader.bufferCursor = buffer;
    reserveHeader.reserved = len;
    return &reserveHeader;
  };

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

  buffioHeader *asyncRead(buffioHeader *header, char *buffer, size_t len,
                          asyncRead then) {
    header->opCode = buffioOpCode::asyncRead;
    header->reqToken.fd = localfd.fd[0];
    header->len.len = len;
    header->data.buffer = buffer;
    header->bufferCursor = buffer;
    header->reserved = 0;
    header->onAsyncDone.onAsyncRead = then;

    if (readReq == nullptr) {
      readReq = header;
      header->next = nullptr;
      header->prev = header;
      return header;
    };
    readReq->prev->next = header;
    header->next = nullptr;
    return header;
  };

  /**
   * @brief async Write to a fd
   * @param[in] header pointer to valid buffioHeader that lifecycle must be
   * valid for async write op
   * @param[in] buffer pointer to the buffer to write
   * @param[in] len size of the data in bytes to write
   *
   * @return buffioHeader crafted for asyncRead
   */

  buffioHeader *asyncWrite(buffioHeader *header, char *buffer, size_t len,
                           asyncWrite then) {

    header->opCode = buffioOpCode::asyncWrite;
    header->reqToken.fd = localfd.fd[0];
    header->len.len = len;
    header->data.buffer = buffer;
    header->onAsyncDone.onAsyncWrite = then;
    header->bufferCursor = buffer;
    header->reserved = len;

    if (writeReq == nullptr) {
      writeReq = header;
      header->next = nullptr;
      header->prev = header;
      return header;
    };

    writeReq->prev->next = header;
    header->next = nullptr;

    return header;
  };

  /**
   *@brief method to add fd to polling
   *
   * @param[in] mask mask of events to watchout on the fd
   *
   * @return buffioHeader Crafter for poll op
   */

  buffioHeader *poll(int mask = EPOLLIN | EPOLLOUT) {
    reserveHeader.opCode = buffioOpCode::poll;
    if (mask == -1)
      reserveHeader.opCode = buffioOpCode::rmPoll;

    reserveHeader.fd = this;
    reserveHeader.len.mask = EPOLLIN;
    reserveHeader.reqToken.fd = localfd.fd[0];
    return &reserveHeader;
  };

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
  void release() {
    auto family = this->fdFamily;
    this->fdFamily = buffioFdFamily::none;
    switch (family) {
    case buffioFdFamily::none:
      assert(family != buffioFdFamily::none);
      break;
    case buffioFdFamily::file:
      // TODO: add support for files
      break;
    case buffioFdFamily::pipe: {
      ::close(localfd.pipeFd[0]);
      ::close(localfd.pipeFd[1]);
    } break;
    case buffioFdFamily::ipv6:
      [[fallthrough]];
    case buffioFdFamily::ipv4: {
      ::close(localfd.sock.socketFd);
    } break;
    case buffioFdFamily::local: {
      if (this->address != nullptr) {
        ::unlink(this->address);
        delete[] this->address;
      };
      ::close(localfd.sock.socketFd);
    } break;
    case buffioFdFamily::fifo: {
      if (this->address != nullptr) {
        ::unlink(this->address);
        delete[] this->address;
      }
    } break;
    };
  };
  friend class buffioFdPool;
  friend class buffioMakeFd;

private:
  /**
   * @brief method to mount the socket created
   * @param[in] address pointer to the address string allocated using new
   * @param[in] socketFd socketfd created by user
   * @param[in] portnumber portnumber of the socket
   * @note addredd is only used for UNIX socket
   * @return void
   */

  void mountSocket(char *address, int socketfd, int portnumber) noexcept {
    localfd.sock.socketFd = socketfd;
    localfd.sock.portnumber = portnumber;
    this->address = address;
    return;
  };
  /*@brief method to mount the pipe
   * @param[in] read read end of the pipe
   * @param[in] write write end of the pipe
   * @return void
   */
  void mountPipe(int read, int write) {
    localfd.pipeFd[0] = read;
    localfd.pipeFd[1] = write;
  };

  /**
   * @brief method to mount the fifo
   * @param[in] address address of the fifo, must be a valid string allocated
   * using new
   * @return void
   */

  void mountFifo(char *address) { this->address = address; };

  buffioFdFamily fdFamily = buffioFdFamily::none;
  buffioOrigin origin = buffioOrigin::routine;

  /**
   * @brief next field helps to chain fd in fdpool
   */

  buffioFd *next = nullptr;
  /**
   * @bried prev field helps to chain fd in fd pool
   *
   */
  buffioFd *prev = nullptr;

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
   */
  buffioHeader reserveHeader;

  /*Are dequeued and given out to batch;
   *only one read/write is supported at any instance of time;
   *when request is done the read/write Req field are marked nullptr
   */

  /**
   * @brief readReq holds the read requests made to the fd
   * @note buffioHeader must be either allocated via the header pool or new
   * operator it's a good practice to use buffiocall to get header memory so the
   * memory leak can be prevented
   */
  buffioHeader *readReq = nullptr;

  /**
   * @brief writeReq holds the write requests made to the fd
   * @note buffioHeader must be either allocated via the header pool or new
   * operator it's a good practice to use buffiocall to get header memory so the
   * memory leak can be prevented
   */

  buffioHeader *writeReq = nullptr;
};

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

class buffioMakeFd {
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
  static int
  createSocket(buffioFd *fdCore, struct sockaddr *lsocket, const char *address,
               int portNumber = 8080,
               buffioFdFamily family = buffioFdFamily::ipv4,
               buffioSocketProtocol protocol = buffioSocketProtocol::tcp,
               bool blocking = false) {

    assert(fdCore != nullptr);
    assert(lsocket != nullptr);
    assert(address != nullptr || portNumber > 0);
    assert(fdCore->fdFamily == buffioFdFamily::none);

    if (portNumber <= 0 || portNumber > 65535)
      return (int)buffioErrorCode::portnumber;

    int domain = 0;
    int type = 0;
    size_t len = 0;
    sockaddr_storage addr = {0};
    socklen_t addrLen = 0;
    char *addressfd = nullptr;

    switch (family) {
    case buffioFdFamily::ipv4: {
      domain = AF_INET;
      type = protocol == buffioSocketProtocol::tcp ? SOCK_STREAM : SOCK_DGRAM;

      sockaddr_in *in4AddrLoc = reinterpret_cast<sockaddr_in *>(&addr);
      if (inet_pton(domain, address, &in4AddrLoc->sin_addr) != 1)
        return (int)buffioErrorCode::socketAddress;

      in4AddrLoc->sin_family = domain;
      in4AddrLoc->sin_port = htons(portNumber);
      addrLen = sizeof(sockaddr_in);
      ::memcpy(lsocket, in4AddrLoc, addrLen);
    } break;

    case buffioFdFamily::ipv6: {
      domain = AF_INET6;
      type = protocol == buffioSocketProtocol::tcp ? SOCK_STREAM : SOCK_DGRAM;

      sockaddr_in6 *in6AddrLoc = reinterpret_cast<sockaddr_in6 *>(&addr);
      if (inet_pton(domain, address, &in6AddrLoc->sin6_addr) != 1)
        return (int)buffioErrorCode::socketAddress;

      in6AddrLoc->sin6_family = domain;
      in6AddrLoc->sin6_port = htons(portNumber);
      addrLen = sizeof(sockaddr_in6);

      ::memcpy(lsocket, in6AddrLoc, addrLen);

    } break;
    case buffioFdFamily::local: {
      domain = AF_UNIX;
      type = protocol == buffioSocketProtocol::tcp ? SOCK_STREAM : SOCK_DGRAM;

      sockaddr_un *unAddrLoc = reinterpret_cast<sockaddr_un *>(&addr);
      len = ::strlen(address);

#ifdef NDEBUG
      if (len > sizeof(unAddrLoc->sun_path)) {
        return (int)buffioErrorCode::socketAddress;
      };

      try {
        addressfd = new char[(len + 1)];
      } catch (std::exception &e) {
        return (int)buffioErrorCode::makeUnique;
      };
#else
      assert(len > sizeof(unAddrLoc->sun_path));
      try {
        addressfd = new char[len + 1];
      } catch (std::exception &e) {
        // error error error
        assert(false);
      };

#endif // NDEBUG

      ::memcpy(addressfd, address, len);
      addressfd[len] = '\0';
      ::memcpy(unAddrLoc->sun_path, addressfd, len + 1);

      addrLen = offsetof(struct sockaddr_un, sun_path) + len + 1;
      ::memcpy(lsocket, unAddrLoc, addrLen);

    } break;
    case buffioFdFamily::raw:
      break;
    default:
      return (int)buffioErrorCode::family;
      break;
    };

    int socketFd = ::socket(domain, type, 0);
    if (socketFd < 0)
      return (int)buffioErrorCode::socket;

    char ip[INET_ADDRSTRLEN];
    sockaddr_in *tmp = (sockaddr_in *)&addr;
    inet_ntop(AF_INET, &tmp->sin_addr, ip, sizeof(ip));

    if (::bind(socketFd, reinterpret_cast<sockaddr *>(&addr), addrLen) != 0) {
      ::close(socketFd);

      return (int)buffioErrorCode::bind;
    }

    fdCore->fdFamily = family;
    fdCore->mountSocket(addressfd, socketFd, portNumber);
    if (blocking == false) {
      buffioMakeFd::setNonBlocking(socketFd);
      fdCore->bitSet(BUFFIO_FD_NON_BLOCKING);
    };
    return (int)buffioErrorCode::none;
  };

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
  static int pipe(buffioFd *fdCore, bool blocking = false) {
    assert(fdCore != nullptr);
    assert(fdCore->fdFamily == buffioFdFamily::none);

    int fdTmp[2];

    if (::pipe(fdTmp) != 0) {
      return (int)buffioErrorCode::pipe;
    }

    fdCore->fdFamily = buffioFdFamily::pipe;
    fdCore->mountPipe(fdTmp[0], fdTmp[1]);

    if (blocking == false) {
      buffioMakeFd::setNonBlocking(fdTmp[0]);
      buffioMakeFd::setNonBlocking(fdTmp[1]);
      fdCore->bitSet(BUFFIO_FD_NON_BLOCKING);
    }
    return (int)buffioErrorCode::none;
  };

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
  static int mkfifo(buffioFd *fdCore,
                    const char *path = "/usr/home/buffioDefault",
                    mode_t mode = 0666, bool onlyFifo = false) {
    assert(fdCore != nullptr);
    assert(fdCore->fdFamily == buffioFdFamily::none);

    if (::mkfifo(path, mode) != 0)
      return (int)buffioErrorCode::fifo;

    size_t len = ::strlen(path);
    char *address = nullptr;
    try {
      address = new char[(len + 1)];
    } catch (std::exception &e) {
      ::unlink(path);
      return (int)buffioErrorCode::makeUnique;
    }

    fdCore->fdFamily = buffioFdFamily::fifo;
    memcpy(address, path, len);
    address[len] = '\0';
    fdCore->mountFifo(address);

    return (int)buffioErrorCode::none;
  }

  /**
   * @brief set a fd to non-blocking mode from blocking mode
   *
   * @param[in] fd the fd to change mode
   * @return return buffioErrorCode::none,on Error value below 0
   * @note return value must not be discarded
   *
   */
  static int setNonBlocking(int fd) {
    if (fd < 0)
      return (int)buffioErrorCode::fd;

    int flags = 0;
    if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
      return (int)buffioErrorCode::fcntl;
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1)
      return (int)buffioErrorCode::fcntl;

    return (int)buffioErrorCode::none;
  };
};

/**
 * @file buffiofd.hpp
 * @author Harsh Sharma
 * @brief Core Memory allocator and deallocator of buffio
 *
 * buffioMemoryPool is a memory manager and allocator for a specific,
 * data types, and provide memory for that type when ever needed.
 *
 */

/**
 * @class buffioFdPool
 * @brief Fd watcher queue, that are currently being in use by program
 *
 * @details
 * buffioFdPool:
 * - Owns every buffioFd, instance
 * - maintains list of buffioFd instance that are being used and is free
 * - prevent opended fd leaks, as when destroyed all the fd are closed properly,
 * via the buffioFd destructor.
 * - help to maintain lifecycle of fd
 *
 * @note the fd that is given back must originate from the pool or must be first
 * pushed to the use queue of the pool, if these condition is not met, the pool
 * becomes corrupt
 */

#include <iostream>
class buffioFdPool {
public:
  /**
   * @brief Constructor of the buffioFdPool,
   *
   * sets default value for the fields
   */
  buffioFdPool() : free(nullptr), inUse(nullptr), count(0) {}

  /**
   * @brief Destroys the buffioFdPool instance, and releases all fd
   */

  ~buffioFdPool() { release(); }

  /**
   * @brief method to mount the memory pool to allocate buffiofd instance
   * @note provide pool, that is initlised correctly. if not error can occur.
   *
   * @param[in] buffioMemoryPool<buffioHeader> pointer to the instance of
   * memorypool
   * @returns void
   *
   */

  void mountPool(buffio::Memory<buffioHeader> *pool) { this->pool = pool; }
  buffioFd *get() {
    buffioFd *tmp = nullptr;
    if (free == nullptr) {
      tmp = new buffioFd;
      tmp->origin = buffioOrigin::pool;
      tmp->next = nullptr;
      tmp->prev = nullptr;
      pushUse(tmp);
      return tmp;
    };

    tmp = free;
    pushUse(tmp);
    free = free->next;
    return tmp;
  };
  /**
   * @brief method check if the pool is empty
   *
   * @note be carefull against the match value.
   * @param[in] match condition for the empty to match count against
   * @return true, if empty condition met, false if not.
   *
   */
  bool empty(int match) const { return (count == match); };

  /**
   * @brief method to release all the fd, manually
   * @warning be carefull with this function, as calling it will close all the
   * active fd.
   *
   * @param[in] void
   * @return void
   *
   */
  void release() {
    _release(inUse);
    inUse = nullptr;
    _release(free);
    free = nullptr;
  };

  /**
   * @brief method to give the buffioFd instance back to the pool for reuse
   *
   * @attention the fd that is given back must originate from the pool or must
   * be first pushed to the use queue of the pool, if these condition is not
   * met, the pool becomes corrupt
   * @param[in] buffioFd the instance that done using.
   * @return void
   *
   */
  void push(buffioFd *tmp) {
    if (tmp->origin != buffioOrigin::pool) {
      popInUse(tmp);
      return;
    };
    tmp->release();
    pushFree(tmp);
  };

  void pushUse(buffioFd *tmp) {
    if (*tmp == BUFFIO_FD_POLLED)
      return;

    count += 1;
    if (inUse == nullptr) {
      inUse = tmp;
      return;
    };
    tmp->next = inUse;
    inUse->prev = tmp;
    inUse = tmp;
  };

private:
  void _release(buffioFd *from) {
    auto tmp = from;
    while (from != nullptr) {
      from = from->next;
      if (tmp->origin == buffioOrigin::pool)
        delete tmp;
      tmp = from;
    }
  };

  void popInUse(buffioFd *tmp) {
    if (tmp->next != nullptr)
      tmp->next->prev = tmp->prev;
    if (tmp->prev != nullptr)
      tmp->prev->next = tmp->next;
    if (tmp == inUse)
      inUse = tmp->next;
    tmp->next = tmp->prev = nullptr;
    count -= 1;
  };
  void pushFree(buffioFd *tmp) {
    if (free == nullptr) {
      free = tmp;
      free->next = free->prev = nullptr;
      return;
    };
    tmp->next = free;
    tmp->prev = nullptr;
    free = tmp;
  };

  buffioFd *free;
  buffioFd *inUse;
  size_t count;
  buffio::Memory<buffioHeader> *pool;
};

#endif
