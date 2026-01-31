#ifndef BUFFIO_SOCK
#define BUFFIO_SOCK

/*
 * Error codes range reserved for buffiosock
 *  [0 - 999]
 *  [Note] Errorcodes are negative value in this range.
 *  0 <= errorcode <= 999
 *  Naming convention is scheduled to change in future to Ocaml style
 */

#include <cassert>
#include <arpa/inet.h>
#include <cstddef>
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
#include <coroutine>

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

  buffioFdFamily fdFamily;
  std::atomic<int> value;
  union {
    struct {
      int socketFd;
      int portnumber;
      char* address;
    } sock;
    
    char *path;
    int fileFd;
    int pipeFd[2];
  } localfd;

  buffioFd() : count(0), token(0),next(nullptr),prev(nullptr) {
    fdFamily = buffioFdFamily::none;
    localfd = {0};
  };
  size_t genReqToken() const { return token.load(std::memory_order_acquire); };
  bool matchToken(size_t token) const {
    return (token == count.load(std::memory_order_acquire));
  }
  inline void markCompletion() noexcept {
    count.fetch_add(1, std::memory_order_acq_rel);
    return;
  };
  void release() {
    switch (fdFamily) {
    case buffioFdFamily::file:
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
      if (localfd.sock.address != nullptr) {
        ::unlink(localfd.sock.address);
        delete []localfd.sock.address;
      };
      ::close(localfd.sock.socketFd);
    } break;
    };
  };
  ~buffioFd(){release();};

/* ==========================================================================================================
 *  buffioFD helper function to create fd
 *
 * ==========================================================================================================
 */

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

  [[nodiscard]]
  int createSocket(const char *address, int portNumber = 8080,
             buffioFdFamily family = buffioFdFamily::ipv4,
             buffioSocketProtocol protocol = buffioSocketProtocol::tcp,
             bool blocking = false) {



    assert(address != nullptr || portNumber > 0);
    assert(fdFamily == buffioFdFamily::none);

   #ifdef NDEBUG
    if(fdFamily != buffioFdFamily::none)
       return (int)buffioErrorCode::occupied;
    if (address == nullptr)
      return (int)buffioErrorCode::socketAddress;

    if (portNumber <= 0 || portNumber > 65535)
      return (int)buffioErrorCode::portnumber;
   #endif
     
    int domain = 0;
    int type = 0;
    size_t len = 0;
    sockaddr_storage addr = {0};
    socklen_t addrLen = 0;

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
        localfd.sock.address = new char[(len + 1)];
      } catch (std::exception &e) {
        return (int)buffioErrorCode::makeUnique;
      };
    #else
      assert(len > sizeof(unAddrLoc->sun_path));
      try {
        localfd.sock.address = new char[len + 1];
      } catch (std::exception &e) {
        //error error error
        assert(false);
      };
       
    #endif // NDEBUG

      ::memcpy(localfd.sock.address, address, len);
      localfd.sock.address[len] = '\0';

      ::memcpy(unAddrLoc->sun_path, localfd.sock.address, len + 1);
      addrLen = offsetof(struct sockaddr_un, sun_path) + len + 1;
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

    this->fdFamily = family;
    localfd.sock.socketFd = socketFd;
    localfd.sock.portnumber = portNumber;

    if (blocking == false)
      setNonBlocking(socketFd);

    return (int)buffioErrorCode::none;
  };


  [[nodiscard]]
  int pipe(bool block = false) {
    assert(fdFamily == buffioFdFamily::none);

    int fdTmp[2];

    if (::pipe(fdTmp) != 0) {
      return (int)buffioErrorCode::pipe;
    }

    this->fdFamily = buffioFdFamily::pipe;
    localfd.pipeFd[0] = fdTmp[0];
    localfd.pipeFd[1] = fdTmp[1];
    if (block == false) {
      setNonBlocking(fdTmp[0]);
      setNonBlocking(fdTmp[1]);
    }
    return (int)buffioErrorCode::none;
  };


  [[nodiscard]]
  int mkfifo(const char *path = "/usr/home/buffioDefault", mode_t mode = 0666,
             bool onlyFifo = false) {

    assert(fdFamily == buffioFdFamily::none);

    if (::mkfifo(path, mode) != 0)
      return (int)buffioErrorCode::fifo;

    size_t len = ::strlen(path);

    try {
      localfd.path = new char[(len + 1)];
    } catch (std::exception &e) {
      ::unlink(path);
      return (int)buffioErrorCode::makeUnique;
    }

    fdFamily = buffioFdFamily::fifo;
    memcpy(localfd.path, path, len);
    localfd.path[len] = '\0';

    return (int)buffioErrorCode::none;
  }

  int setNonBlocking(int fd) {
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

/* ==========================================================================================================
 *  buffioFD helper end
 *
 * ==========================================================================================================
 */
  friend class buffioFdPool;
private:
  std::atomic<size_t> count;
  std::atomic<size_t> token;
  buffioFd *next;
  buffioFd *prev;

};

class buffioFdPool{

public:
  buffioFdPool():free(nullptr),inUse(nullptr){}
  ~buffioFdPool(){}
  buffioFd *pop(){

   buffioFd *tmp = nullptr;
   if(free == nullptr){
     tmp  = new buffioFd;
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
  void release(){
     _release(inUse); 
     _release(free);
  };
  void push(buffioFd *tmp){
    popInUse(tmp);
    tmp->release();
    pushFree(tmp);
  };
private:
 void _release(buffioFd *from){
   auto tmp = from;
    while(from != nullptr){
       from = from->next;
       delete tmp;
       tmp = from;
    }
  };

 void popInUse(buffioFd *tmp){
    if(tmp->next != nullptr)
      tmp->next->prev = tmp->prev;
    if(tmp->prev != nullptr)
      tmp->prev->next = tmp->next;
    if(tmp == inUse)
      inUse = tmp->next;
     tmp->next = tmp->prev = nullptr;
  };
 void pushFree(buffioFd *tmp){
  if(free == nullptr){
      free = tmp;
      free->next = free->prev = nullptr;
      return;
    };
   tmp->next = free;
   tmp->prev = nullptr;
   free = tmp;
 };
 void pushUse(buffioFd *tmp){
   if(inUse == nullptr){
      inUse = tmp;
      return;
    };
    tmp->next = inUse;
    inUse->prev = tmp;
    inUse = tmp;
  };

 buffioFd *free;
 buffioFd *inUse;
};

typedef struct buffioHeader {
  uint8_t opCode;
  uint8_t buffioSyscall;
  uint8_t handleType;
  uint16_t relayid;
  /*
   * reserved for future use
   */
  uint8_t reserved;
  uint8_t reserved_;
  // buffioReservedFields

   buffioFd *fd;
} buffioHeader;


/* supported protocol string format:
 * tcp - "tcp://127.0.0.1:8080".
 * udp - "udp://127.0.0.1:8081".
 * file - "file://path:usr/mytype.txt".
 * fifo - "fifo://path:/home/usr/fifo".
 * pipe - "pipe://path:null".
 */


typedef struct buffioTimer{
 uint64_t duration;
 int repeat;
 int self;
 std::coroutine_handle<> handle;
}buffioTimer;


#endif
