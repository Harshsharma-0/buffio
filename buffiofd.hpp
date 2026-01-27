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
  buffioFdFamily family;
  std::atomic<int> value; // mask that contain the type of request
  buffioFdReq request;

  union {
    struct {
      int socketFd;
      int portnumber;
    } sock;
    int fileFd;
    int pipeFd[2];
  };

  int fds[2]; // additional space for pipe also;
  std::unique_ptr<char[]> address;
  buffioFdInfo() : address(nullptr), count(0), token(0) {
    family = buffioFdFamily::none;
  };
  size_t genReqToken() const { return token.load(std::memory_order_acquire); };
  bool matchToken(size_t token) const {
    return (token == count.load(std::memory_order_acquire));
  }
  inline void markCompletion() noexcept {
    count.fetch_add(1, std::memory_order_acq_rel);
    return;
  };
  ~buffioFdInfo() {
    switch (family) {
    case buffioFdFamily::file:
      break;
    case buffioFdFamily::pipe: {
      ::close(fds[0]);
      ::close(fds[1]);
    } break;
    case buffioFdFamily::ipv6:
      [[fallthrough]];
    case buffioFdFamily::ipv4: {
      ::close(fds[0]);
    } break;
    case buffioFdFamily::local: {
      if (address != nullptr) {
        ::unlink((const char *)address.get());
      };
      ::close(fds[0]);
    } break;
    };
  };

private:
  std::atomic<size_t> count;
  std::atomic<size_t> token;
};

using buffioFdView = std::shared_ptr<buffioFdInfo>;
using buffioFdViewWeak = std::weak_ptr<buffioFdInfo>;

class buffioFd {

public:
  // function to create a ipsocket, blocking for future update
  buffioFd() {}
  buffioFd &operator=(const buffioFd &) = default;

  [[nodiscard]]
  int createSocket(const char *address = nullptr, int portNumber = 8080,
                   buffioFdFamily family = buffioFdFamily::ipv4,
                   buffioSocketProtocol protocol = buffioSocketProtocol::tcp,
                   bool blocking = true) {

    if (address == nullptr)
      return (int)buffioErrorCode::socketAddress;

    if (portNumber <= 0 || portNumber > 65535)
      return (int)buffioErrorCode::portnumber;

    // when there is no ownership the count is  0
    if (fdData.use_count() != 0)
      return (int)buffioErrorCode::occupied;

    try {
      fdData = std::make_shared<buffioFdInfo>();
    } catch (std::exception &e) {
      return (int)buffioErrorCode::makeShared;
    }

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
      if (len >= sizeof(unAddrLoc->sun_path)) {
        fdData.reset();
        return (int)buffioErrorCode::socketAddress;
      };

      try {
        fdData->address = std::make_unique<char[]>(len + 1);
      } catch (std::exception &e) {
        fdData.reset();
        return (int)buffioErrorCode::makeUnique;
      };

      ::memcpy(fdData->address.get(), address, len);
      fdData->address[len] = '\0';

      ::memcpy(unAddrLoc->sun_path, address, len + 1);
      addrLen = offsetof(struct sockaddr_un, sun_path) +
                sizeof(sockaddr_un::sun_path);
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

    fdData->family = family;
    fdData->fds[0] = socketFd;
    fdData->fds[1] = portNumber;
    return (int)buffioErrorCode::none;
  };

  [[nodiscard]]
  int createPipe(bool block = true) {

    if (fdData.use_count() != 0)
      return (int)buffioErrorCode::occupied;

    int fdTmp[2];

    try {
      fdData = std::make_shared<buffioFdInfo>();
    } catch (std::exception &e) {
      return (int)buffioErrorCode::makeShared;
    };

    if (::pipe(fdTmp) != 0) {
      fdData.reset();
      return (int)buffioErrorCode::pipe;
    }

    fdData->family = buffioFdFamily::pipe;
    fdData->fds[0] = fdTmp[0];
    fdData->fds[1] = fdTmp[1];
    return (int)buffioErrorCode::none;
  };

  [[nodiscard]]
  int createfifo(const char *path = "/usr/home/buffioDefault",
                 mode_t mode = 0666, bool onlyFifo = false) {

    if (fdData.use_count() != 0)
      return (int)buffioErrorCode::occupied;

    if (::mkfifo(path, mode) != 0)
      return (int)buffioErrorCode::fifo;

    if (onlyFifo == true)
      return (int)buffioErrorCode::none;

    try {
      fdData = std::make_shared<buffioFdInfo>();
      fdData->family = buffioFdFamily::fifo;
    } catch (std::exception &e) {
      return (int)buffioErrorCode::makeShared;
    }

    /*
    1 2 3 4 5 6 7
    0 1 2 3 4 5 6
    */

    size_t len = ::strlen(path);

    try {
      fdData->address = std::make_unique<char[]>(len + 1);
    } catch (std::exception &e) {
      fdData.reset();
      ::unlink(path);
      return (int)buffioErrorCode::makeUnique;
    }
    memcpy(fdData->address.get(), path, len);
    fdData->address[len] = '\0';

    return (int)buffioErrorCode::none;
  }

  /*
  [[nodiscard]] buffioFdView openfile(const cha *path = nullptr,
                                      int mode = 0) {
    // TODO: add file support
    if (path == nullptr)
      error = -1;

    error = 0;
    return 0;
  };
*/

  buffioFdView fdData;
};

#endif
