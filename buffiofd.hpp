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
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#if !defined(BUFFIO_IMPLEMENTATION)
#include "buffioenum.hpp" // header for opcode
#include "buffiomemory.hpp"
#endif

#include <atomic>
#include <memory>

enum class buffioSocketFamily : int {
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

enum class buffioFdError : int {
  none = 0,
  socket = -101,
  bind = -102,
  open = -103,
  file_path = -104,
  socketAddress = -105,
  portnumber = -106,
  pipe = -107,
  fifo = -108,
  fifo_path = -109,
  occupied = -110,
  family = -112,
  fcntl = -113
};

struct buffioFdReq_rw {
  buffio_fd_opcode opcode;
  char *buffer;
  size_t len;
  void *data;
};

struct buffioFdReq_add {
  buffio_fd_opcode opcode;
  int fd;
  void *data;
};

struct buffioFdReq_paged {
  buffio_fd_opcode opcode;
  buffiopage *page;
};

union buffioFdReq {
  struct buffioFdReq_rw rw;
  struct buffioFdReq_paged paged;
  struct buffioFdReq_add add;
};

class buffioFdInfo {

public:
  buffioSocketFamily family;
  std::atomic<int> value; // mask that contain the type of request
  buffioFdReq request;

  int fds[2]; // additional space for pipe also;
  std::unique_ptr<char[]> path;

  ~buffioFdInfo() {
    switch (family) {
    case buffioSocketFamily::file:
      break;
    case buffioSocketFamily::pipe: {
      ::close(fds[0]);
      ::close(fds[1]);
    } break;
    case buffioSocketFamily::ipv6:
      [[fallthrough]];
    case buffioSocketFamily::ipv4: {
      ::close(fds[0]);
    } break;
    case buffioSocketFamily::local: {
      ::unlink((const char *)path.get());
      ::close(fds[0]);
    } break;
    };
  };
};

using buffioFdView = std::shared_ptr<buffioFdInfo>;
using buffioFdViewWeak = std::weak_ptr<buffioFdInfo>;

class buffiofd {

public:
  // function to create a ipsocket, blocking for future update
  buffiofd() { error = buffioFdError::none; }
  buffiofd &operator=(const buffiofd &) = default;

  [[nodiscard]]
  buffioFdView
  createSocket(const char *address = nullptr, int portNumber = 8080,
               buffioSocketFamily family = buffioSocketFamily::ipv4,
               buffioSocketProtocol protocol = buffioSocketProtocol::tcp,
               bool blocking = true) {

    if (address == nullptr) {
      error = buffioFdError::socketAddress;
      return nullptr;
    };

    if (portNumber <= 0 || portNumber > 65535) {
      error = buffioFdError::portnumber;
      return nullptr;
    }
    std::unique_ptr<char[]> addressPtr = nullptr;

    int domain = static_cast<int>(family);
    int type = static_cast<int>(protocol);

    sockaddr_storage addr = {0};
    socklen_t addrLen = 0;

    switch (family) {
    case buffioSocketFamily::ipv4: {
      domain = AF_INET;
      type = protocol == buffioSocketProtocol::tcp ? SOCK_STREAM : SOCK_DGRAM;

      sockaddr_in *in4AddrLoc = reinterpret_cast<sockaddr_in *>(&addr);
      if (inet_pton(domain, address, &in4AddrLoc->sin_addr) != 1) {
        error = buffioFdError::socketAddress;
        return nullptr;
      };
      size_t len = ::strlen(address);
      std::unique_ptr<char[]> tmpPtrIn = std::make_unique<char[]>(len + 1);
      ::memcpy(tmpPtrIn.get(), address, (len + 1));
      addressPtr = std::move(tmpPtrIn);

      in4AddrLoc->sin_family = domain;
      in4AddrLoc->sin_port = htons(portNumber);
      addrLen = sizeof(sockaddr_in);
    } break;

    case buffioSocketFamily::ipv6: {
      domain = AF_INET6;
      type = protocol == buffioSocketProtocol::tcp ? SOCK_STREAM : SOCK_DGRAM;

      sockaddr_in6 *in6AddrLoc = reinterpret_cast<sockaddr_in6 *>(&addr);
      if (inet_pton(domain, address, &in6AddrLoc->sin6_addr) != 1) {
        error = buffioFdError::socketAddress;
        return nullptr;
      }
      size_t len = ::strlen(address);
      std::unique_ptr<char[]> tmpPtrIn6 = std::make_unique<char[]>(len + 1);
      ::memcpy(tmpPtrIn6.get(), address, (len + 1));
      addressPtr = std::move(tmpPtrIn6);

      in6AddrLoc->sin6_family = domain;
      in6AddrLoc->sin6_port = htons(portNumber);
      addrLen = sizeof(sockaddr_in6);

    } break;
    case buffioSocketFamily::local: {
      domain = AF_UNIX;
      type = protocol == buffioSocketProtocol::tcp ? SOCK_STREAM : SOCK_DGRAM;

      sockaddr_un *unAddrLoc = reinterpret_cast<sockaddr_un *>(&addr);
      size_t len = ::strlen(address);
      if (len >= sizeof(unAddrLoc->sun_path)) {
        error = buffioFdError::socketAddress;
        return nullptr;
      };
      std::unique_ptr<char[]> tmpPtrUn = std::make_unique<char[]>(len + 1);
      addressPtr = std::move(tmpPtrUn);

      ::memcpy(unAddrLoc->sun_path, address, len + 1);
      addrLen = sizeof(sockaddr_un);
    } break;
    case buffioSocketFamily::raw:
      break;
    default:
      return nullptr;
      break;
    };

    int socketFd = ::socket(domain, type, 0);
    if (socketFd < 0) {
      error = buffioFdError::socket;
      return nullptr;
    };

    if (::bind(socketFd, reinterpret_cast<sockaddr *>(&addr), addrLen) != 0) {
      error = buffioFdError::bind;
      return nullptr;
    };
    buffioFdView fdPtr = std::make_shared<buffioFdInfo>();
    if (fdPtr.get() == nullptr)
      return nullptr;

    fdPtr->path = std::move(addressPtr);
    fdPtr->family = family;
    fdPtr->fds[0] = socketFd;
    return fdPtr;
  };

  [[nodiscard]]
  buffioFdView createPipe(bool block = true) {
    int fdTmp[2];

    if (::pipe(fdTmp) != 0)
      error = buffioFdError::pipe;

    buffioFdView fdPtr = std::make_shared<buffioFdInfo>();
    if (fdPtr.get() == nullptr)
      return nullptr;

    fdPtr->family = buffioSocketFamily::pipe;
    fdPtr->fds[0] = fdTmp[0];
    fdPtr->fds[1] = fdTmp[1];
    error = buffioFdError::none;

    return fdPtr;
  };

  [[nodiscard]]
  buffioFdView createfifo(const char *path = nullptr, mode_t mode = 0666) {

    if (mkfifo(path, mode) == 0)
      error = buffioFdError::none;

    buffioFdView fdPtr = std::make_shared<buffioFdInfo>();
    if (fdPtr.get() == nullptr)
      return nullptr;

    size_t len = ::strlen(path);
    std::unique_ptr<char[]> tmpPtr = std::make_unique<char[]>(len + 1);
    ::memcpy(tmpPtr.get(), path, (len + 1));
    fdPtr->path = std::move(tmpPtr);

    error = buffioFdError::fifo;
  }

  /*
    [[nodiscard]] buffioFdView openfile(const char *path = nullptr,
                                        int mode = 0) {
      // TODO: add file support
      if (path == nullptr)
        error = -1;

      error = 0;
      return 0;
    };
  */

  buffioFdError error;
};

#endif
