#ifndef BUFFIO_SOCK
#define BUFFIO_SOCK

/*
 * Error codes range reserved for buffiosock
 *  [0 - 999]
 *  [Note] Errorcodes are negative value in this range.
 *  0 <= errorcode <= 999
 *  Naming convention is scheduled to change in future to Ocaml style
 */

#include <arpa/inet.h>
#include <cassert>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <errno.h>
#include <exception>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#if !defined(BUFFIO_IMPLEMENTATION)
// #include "buffioenum.hpp" // header for opcode
#include "buffiomemory.hpp"
#endif

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

class buffioFd {

public:

  /*
   * operator overload to set bits in the mask
   * of fd.
   */

  void operator|(int bit){ rwmask |= bit;}
  /* 
   * operator overload to check if the bit field
   * is set or not.
   *
   */
  bool operator == (int bit)const{
    return (rwmask & bit);
  };

  /*
   * bitSet, method set a specific bit in 
   * the rwmask.
   */ 

  void bitSet(int bit)noexcept{
    rwmask |= bit;
  };

  /*
   * isBitsSet, checks if the specific bit is
   * set or not.
   *
   */

  bool isBitSet(int bit)const{
    return (rwmask & bit);
  };

  /*
   * function to unset the bit in the rwmask.
   */

  void unsetBit(int bit)noexcept{ 
    rwmask &= ~(bit);
  };

  /*
   * constructor of the buffiofd
   */

  buffioFd() : next(nullptr), prev(nullptr){
    fdFamily = buffioFdFamily::none;
    origin = buffioOrigin::routine;
    address = nullptr;
    rwmask = 0;
    reserveHeader.fd = this;
    readReq = writeReq = nullptr;
    prev = next = nullptr; 
    localfd = {0};
  };

   buffioHeader *getHeader()const{
    return nullptr; 
   };
   buffioHeader *getReadReq()const{
     return readReq; 
   };
   buffioHeader *getWriteReq()const{
     return writeReq;
   };
   const buffioHeader *getReserveHeader()const{
    return &reserveHeader;
   };
   inline void popReq()noexcept{ 
    return;
   }

   int listen(int backlog)const{ 
    return ::listen(localfd.fd[0],backlog);
   };

   int accept(struct sockaddr *addr,socklen_t *socklen)const{
    return ::accept(localfd.fd[0],addr,socklen);
   };

   int connect(struct sockaddr *addr,socklen_t socklen)const{
    return ::connect(localfd.fd[0],addr,socklen);
   };
    
   buffioHeader *asyncAccept(struct sockaddr *addr,socklen_t socklen,
                             buffioOnAsyncAccept then){
   
    /*
     * header is copied via the the promise object internal machanism
     */

     reserveHeader.opCode = buffioOpCode::asyncAccept;
     reserveHeader.reqToken.fd = localfd.fd[0];
     reserveHeader.len.socklen = socklen;
     reserveHeader.data.socketaddr = addr;
     reserveHeader.onAsyncDone.onAsyncAccept = then;
     
     return &reserveHeader;
   };

   buffioHeader *asyncConnect(struct sockaddr *addr,socklen_t socklen,
                              buffioOnAsyncConnect then){
    reserveHeader.opCode = buffioOpCode::asyncConnect;
    reserveHeader.reqToken.fd = localfd.fd[0];
    reserveHeader.len.socklen = socklen;
    reserveHeader.data.socketaddr = addr;
    reserveHeader.onAsyncDone.onAsyncConnect = then;
    return &reserveHeader;
   };

   buffioHeader *waitAccept(struct sockaddr *addr,socklen_t socklen){ 
     reserveHeader.opCode = buffioOpCode::waitAccept;
     reserveHeader.reqToken.fd = localfd.fd[0];
     reserveHeader.len.socklen = socklen;
     reserveHeader.data.socketaddr = addr;
     return &reserveHeader;
   };

   buffioHeader *waitConnect(struct sockaddr *addr,socklen_t socklen){ 
     reserveHeader.opCode = buffioOpCode::waitConnect;
     reserveHeader.reqToken.fd = localfd.fd[0];
     reserveHeader.len.socklen = socklen;
     reserveHeader.data.socketaddr = addr;
     return &reserveHeader;
   };

  /*
   * generic read/write methods for buffiofd, if user want to 
   * read in the routine, not depending on the eventloop.
   *
   */

  ssize_t read(char *buffer, size_t len) const{ 
    return ::read(localfd.fd[0],buffer,len); 
  };
  ssize_t write(char *buffer, size_t len) const{
    return ::write(localfd.fd[0],buffer,len); 
  };


  /*
   * waitRead/waitWrite returns back if there is data available to read 
   * from the fd, or it there not any data read or fd is not ready to write
   * then the request is queued in the fd class and when there event 
   * available, it is serrved.
   */

  buffioHeader *waitRead(char *buffer, size_t len) {
     reserveHeader.opCode = buffioOpCode::read;
     reserveHeader.reqToken.fd = localfd.fd[0];
     reserveHeader.len.len = len;
     reserveHeader.data.buffer = buffer;
     return &reserveHeader;
  };
  buffioHeader *waitWrite(char *buffer, size_t len) {
     reserveHeader.opCode = buffioOpCode::read;
     reserveHeader.reqToken.fd = localfd.fd[0];
     reserveHeader.len.len = len;
     reserveHeader.data.buffer = buffer;
     return &reserveHeader;
  };

 /*
   * asyncRead/asyncWrite behaves same as the waitRead/waitWrite, the difference is that
   * asyncRead/asyncWrite, let you push task/routine handle that should be run after, the
   * operation is done.
   *
   */


  buffioHeader *asyncRead(char *buffer, size_t len,
                          buffioOnAsyncRead then) {
    reserveHeader.opCode = buffioOpCode::asyncRead;
    reserveHeader.reqToken.fd = localfd.fd[0];
    reserveHeader.len.len = len;
    reserveHeader.data.buffer = buffer;
    reserveHeader.onAsyncDone.onAsyncRead = then;
    return &reserveHeader;
  };

  buffioHeader *asyncWrite(char *buffer, size_t len,
                           buffioOnAsyncWrite then) {

    reserveHeader.opCode = buffioOpCode::asyncWrite;
    reserveHeader.reqToken.fd = localfd.fd[0];
    reserveHeader.len.len = len;
    reserveHeader.data.buffer = buffer;
    reserveHeader.onAsyncDone.onAsyncWrite = then;

    return &reserveHeader;
  };

 

  buffioHeader *rmPoll() {
    assert(fdFamily != buffioFdFamily::none);
    reserveHeader.opCode = buffioOpCode::rmPoll;
    reserveHeader.reqToken.fd = localfd.fd[0];
    return &reserveHeader;
  };

  int getPipeRead() const { return localfd.pipeFd[0]; }
  int getPipeWrite() const { return localfd.pipeFd[1]; }

  /*
    uint32_t genReqToken()noexcept{
      return token.fetch_add(1,std::memory_order_acq_rel);
    };
    bool matchToken(uint32_t token) const {
      return (token == count.load(std::memory_order_acquire));
    }
    inline void markCompletion() noexcept {
      count.fetch_add(1, std::memory_order_acq_rel);
      return;
    };
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
  ~buffioFd() { release(); };
  /*
    [[nodiscard("buffioFd errors must be handled")]]
    int open(const char *protocol = nullptr) {
      if (protocol == nullptr)
        return (int)buffioErrorCode::protocolString;
      size_t len = ::strlen(protocol);
      if (len <= 7)
        return (int)buffioErrorCode::protocol;

      // todo add support for string based opening of fds;
      return 0;
    };

  */
  friend class buffioFdPool;
  friend class buffioMakeFd;
private:
  void mountSocket(char *address, int socketfd, int portnumber) noexcept {
    localfd.sock.socketFd = socketfd;
    localfd.sock.portnumber = portnumber;
    this->address = address;
    return;
  };
  void mountPipe(int read, int write) {
    localfd.pipeFd[0] = read;
    localfd.pipeFd[1] = write;
  };
  void mountFifo(char *address) { this->address = address; };
  buffioFdFamily fdFamily;
  buffioOrigin origin;

  /*
   * fields to support fdpool
   */
  buffioFd *next; 
  buffioFd *prev;
  
protected:
 

 /*
   * rwmask define the readyness of the file descriptor.
   * see buffiocommon.hpp to see the mask defined for readyness
   * of the fd,
   * 
   * - BUFFIO_READ_READY: if masked with this, it means the fd is
   *          ready for read operation.
   *
   * - BUFFIO_WRITE_READY: if masked with this, it means the fd is
   *          ready for write operation.
   * 
   */

  int rwmask; 
 

  char *address;

  union {
    struct {
      int socketFd;
      int portnumber;
    } sock;
    int fileFd;
    int pipeFd[2];
    int fd[2];
  } localfd;

  //never goes to the batch
  buffioHeader  reserveHeader; 

  /*Are dequeued and given out to batch;
   *only one read/write is supported at any instance of time;
   *when request is done the read/write Req field are marked nullptr
   */

  buffioHeader *readReq;
  buffioHeader *writeReq;
 };

class buffioMakeFd {
public:
  [[nodiscard]]
  static int
  createSocket(buffioFd *fdCore,
               struct sockaddr *lsocket,
               const char *address, 
               int portNumber = 8080,
               buffioFdFamily family = buffioFdFamily::ipv4,
               buffioSocketProtocol protocol = buffioSocketProtocol::tcp,
               bool blocking = false){

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
      ::memcpy(lsocket,in4AddrLoc,addrLen);
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

      ::memcpy(lsocket,in6AddrLoc,addrLen);

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
      ::memcpy(lsocket,unAddrLoc,addrLen);

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

    if (::bind(socketFd, reinterpret_cast<sockaddr *>(&addr), addrLen) != 0) {
      ::close(socketFd);
      return (int)buffioErrorCode::bind;
    }

    fdCore->fdFamily = family;
    fdCore->mountSocket(addressfd, socketFd, portNumber);
    if (blocking == false)
      buffioMakeFd::setNonBlocking(socketFd);

    return (int)buffioErrorCode::none;
  };

  [[nodiscard]]
  static int pipe(buffioFd *fdCore, bool block = false) {

    assert(fdCore != nullptr);
    assert(fdCore->fdFamily == buffioFdFamily::none);

    int fdTmp[2];

    if (::pipe(fdTmp) != 0) {
      return (int)buffioErrorCode::pipe;
    }

    fdCore->fdFamily = buffioFdFamily::pipe;
    fdCore->mountPipe(fdTmp[0], fdTmp[1]);

    if (block == false) {
      buffioMakeFd::setNonBlocking(fdTmp[0]);
      buffioMakeFd::setNonBlocking(fdTmp[1]);
    }
    return (int)buffioErrorCode::none;
  };

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

class buffioFdPool {

public:
  buffioFdPool() : free(nullptr), inUse(nullptr) {}
  ~buffioFdPool() {}
  void mountPool(buffioMemoryPool<buffioHeader> *pool) { this->pool = pool; }
  buffioFd *get(){

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

  void release() {
    _release(inUse);
    _release(free);
  };
  void push(buffioFd *tmp) {
    if (tmp->origin != buffioOrigin::pool) {
      popInUse(tmp);
      return;
    };
    tmp->release();
    pushFree(tmp);
  };
  void pushUse(buffioFd *tmp) {
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
  buffioMemoryPool<buffioHeader> *pool;
};

#endif
