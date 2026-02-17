#include "buffio/fd.hpp"
#include "buffio/enum.hpp"
#include "buffio/fiber.hpp"
#include <asm-generic/errno.h>
#include <cerrno>
#include <cstring>

namespace buffio {

int MakeFd::socket(buffio::Fd &fdCore, struct sockaddr *lsocket,
                   const char *address, bool bindSocket, int portNumber,
                   buffioFdFamily family, buffioSocketProtocol protocol,
                   bool blocking) {

  assert(lsocket != nullptr);
  assert(address != nullptr || portNumber > 0);
  assert(fdCore.fdFamily == buffioFdFamily::none);

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

  if (bindSocket == true) {
    if (::bind(socketFd, reinterpret_cast<sockaddr *>(&addr), addrLen) != 0) {
      ::close(socketFd);

      return (int)buffioErrorCode::bind;
    }
  };

  fdCore.fdFamily = family;
  fdCore.mountSocket(addressfd, socketFd, portNumber);
  if (blocking == false) {
    buffio::MakeFd::setNonBlocking(socketFd);
    fdCore.bitSet(BUFFIO_FD_NON_BLOCKING);
  };
  return (int)buffioErrorCode::none;
};

int MakeFd::pipe(buffio::Fd &fdCore, bool blocking) {

  assert(fdCore.fdFamily == buffioFdFamily::none);

  int fdTmp[2];

  if (::pipe(fdTmp) != 0) {
    return (int)buffioErrorCode::pipe;
  }

  fdCore.fdFamily = buffioFdFamily::pipe;
  fdCore.mountPipe(fdTmp[0], fdTmp[1]);

  if (blocking == false) {
    buffio::MakeFd::setNonBlocking(fdTmp[0]);
    buffio::MakeFd::setNonBlocking(fdTmp[1]);
    fdCore.bitSet(BUFFIO_FD_NON_BLOCKING);
  }
  return (int)buffioErrorCode::none;
};

int MakeFd::mkfifo(buffio::Fd &fdCore, const char *path, mode_t mode,
                   bool onlyFifo) {

  assert(fdCore.fdFamily == buffioFdFamily::none);

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

  fdCore.fdFamily = buffioFdFamily::fifo;
  memcpy(address, path, len);
  address[len] = '\0';
  fdCore.mountFifo(address);

  return (int)buffioErrorCode::none;
};
int MakeFd::setNonBlocking(int fd) {
  if (fd < 0)
    return (int)buffioErrorCode::fd;

  int flags = 0;
  if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
    return (int)buffioErrorCode::fcntl;
  if (flags & O_NONBLOCK)
    return (int)buffioErrorCode::none;
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1)
    return (int)buffioErrorCode::fcntl;

  return (int)buffioErrorCode::none;
}
int MakeFd::mkFdSock(buffio::Fd &fdCore, int fd, struct sockaddr &addr) {

  assert(fd >= 0);

  int portNumber = 0;
  switch (addr.sa_family) {
  case AF_UNIX:
    fdCore.fdFamily = buffioFdFamily::local;
  case AF_INET:
    fdCore.fdFamily = buffioFdFamily::ipv4;
    portNumber = ntohs(((sockaddr_in *)&addr)->sin_port);
  case AF_INET6:
    fdCore.fdFamily = buffioFdFamily::ipv6;
    portNumber = ntohs(((sockaddr_in6 *)&addr)->sin6_port);
    break;
  default:
    assert(false);
    break;
  };
  //  fdCore | BUFFIO_WRITE_READY;
  MakeFd::setNonBlocking(fd);
  fdCore.mountSocket(nullptr, fd, portNumber);

  return 0;
};

int MakeFd::openFile(buffio::Fd &fdCore, const char *path, int flags,
                     mode_t mode) {

  int fd = -1;
  if ((fd = open(path, flags | O_CLOEXEC, mode)) < 0)
    return (int)buffioErrorCode::open;

  fdCore.mountFile(fd);

  return 0;
};
}; // namespace buffio

namespace buffio {

Fd::~Fd() { release(); }

buffioRoutineStatus Fd::asyncConnect(onAsyncConnects then) {
  reserveHeader.opCode = buffioOpCode::asyncConnect;
  reserveHeader.fd = this;
  reserveHeader.reqToken.fd = localfd.fd[0];
  return buffioRoutineStatus::none;
};

buffioHeader *Fd::waitAccept(struct sockaddr *addr, socklen_t len) {
  if (!(rwmask & BUFFIO_FD_ACCEPT_READY)) {
    buffio::fiber::poller->pollMod(localfd.fd[0], this, EPOLLIN | EPOLLET);
    rwmask |= BUFFIO_FD_ACCEPT_READY;
  };

  reserveHeader.opCode = buffioOpCode::waitAccept;
  reserveHeader.fd = this;
  reserveHeader.reqToken.fd = localfd.fd[0];
  reserveHeader.data.socketaddr = addr;
  reserveHeader.len.socklen = len;

  if (rwmask & BUFFIO_READ_READY) {
    reserveHeader.rwtype = buffioReadWriteType::async;
    buffio::fiber::requestBatch->pushHead(&reserveHeader);
    return &reserveHeader;
  } else {
    buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
  }
  return &reserveHeader;
};

buffioHeader *Fd::reserveToQueue(int rmBit) {
  if (rwmask & rmBit) {
    auto header = getRawHeader();
    *header = reserveHeader;
    this->unsetBit(rmBit);
    buffio::fiber::requestBatch->push(header);
    ::memset(&reserveHeader, '\0', sizeof(buffioHeader));
    reserveHeader.opCode = buffioOpCode::none;
    return header;
  };

  return nullptr;
};
buffioHeader *Fd::waitConnect(struct sockaddr *addr, socklen_t socklen) {

  if (this->connect(addr, socklen) != -1)
    return nullptr;

  switch (errno) {
  case EINPROGRESS: {
    reserveHeader.opCode = buffioOpCode::waitConnect;
    reserveHeader.unsetBit = BUFFIO_READ_READY;
    reserveHeader.fd = this;
    reserveHeader.reqToken.fd = localfd.fd[0];
    reserveHeader.data.socketaddr = addr;
    reserveHeader.len.socklen = socklen;
    reserveHeader.opError = 0;
    buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
    buffio::fiber::poller->pollMod(localfd.fd[0], this, EPOLLOUT);
    return &reserveHeader;
  } break;
  default:
    reserveHeader.opError = -1;
    break;
  };

  return nullptr;
};

buffioHeader *Fd::waitRead(char *buffer, size_t len) {
  reserveHeader.opCode = fdFamily == buffioFdFamily::file
                             ? buffioOpCode::readFile
                             : buffioOpCode::read;
  reserveHeader.fd = this;
  reserveHeader.unsetBit = BUFFIO_READ_READY;
  reserveHeader.rwtype = buffioReadWriteType::read;
  reserveHeader.data.buffer = buffer;
  reserveHeader.bufferCursor = buffer;
  reserveHeader.reserved = len;
  reserveHeader.len.len = len;
  reserveHeader.reqToken.fd = localfd.fd[0];

  if (fdFamily == buffioFdFamily::file) {
    buffio::fiber::poller->push(&reserveHeader);
    buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
    return &reserveHeader;
  };

  if (rwmask & BUFFIO_READ_READY) {
    buffio::fiber::requestBatch->push(&reserveHeader);
    return &reserveHeader;
  };

  buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
  return &reserveHeader;
};

buffioHeader *Fd::waitWrite(char *buffer, size_t len) {
  reserveHeader.opCode = fdFamily == buffioFdFamily::file
                             ? buffioOpCode::writeFile
                             : buffioOpCode::write;

  reserveHeader.fd = this;
  reserveHeader.data.buffer = buffer;
  reserveHeader.unsetBit = BUFFIO_WRITE_READY;
  reserveHeader.rwtype = buffioReadWriteType::write;
  reserveHeader.bufferCursor = buffer;
  reserveHeader.reserved = len;
  reserveHeader.len.len = len;
  reserveHeader.reqToken.fd = localfd.fd[0];
  if (fdFamily == buffioFdFamily::file) {
    buffio::fiber::poller->push(&reserveHeader);
    buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
    return &reserveHeader;
  };

  if (rwmask & BUFFIO_WRITE_READY) {
    buffio::fiber::requestBatch->push(&reserveHeader);
    return &reserveHeader;
  };

  buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
  return &reserveHeader;
};

buffioRoutineStatus Fd::asyncRead(char *buffer, size_t len, onAsyncReads then) {
  auto header = getRawHeader();

  header->opCode = fdFamily == buffioFdFamily::file
                       ? buffioOpCode::asyncReadFile
                       : buffioOpCode::asyncRead;

  header->fd = this;
  header->unsetBit = BUFFIO_READ_READY;
  header->rwtype = buffioReadWriteType::read;
  header->data.buffer = buffer;
  header->bufferCursor = buffer;
  header->reserved = len;
  header->len.len = len;
  header->reqToken.fd = localfd.fd[0];
  header->onAsyncDone.onAsyncRead = then;

  if (fdFamily == buffioFdFamily::file) {
    buffio::fiber::poller->push(header);
    buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
    buffio::fiber::poller->ping();
    return buffioRoutineStatus::none;
  };

  if (rwmask & BUFFIO_READ_READY) {

    this->unsetBit(BUFFIO_READ_READY);
    buffio::fiber::requestBatch->push(header);

    return buffioRoutineStatus::none;
  }
  buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
  pendingReadReq = header;

  return buffioRoutineStatus::none;
};

buffioRoutineStatus Fd::asyncWrite(char *buffer, size_t len,
                                   onAsyncWrites then) {
  auto header = getRawHeader();

  header->opCode = fdFamily == buffioFdFamily::file
                       ? buffioOpCode::asyncWriteFile
                       : buffioOpCode::asyncWrite;

  header->rwtype = buffioReadWriteType::write;
  header->fd = this;
  header->reqToken.fd = localfd.fd[0];
  header->unsetBit = BUFFIO_WRITE_READY;
  header->data.buffer = buffer;
  header->len.len = len;
  header->onAsyncDone.onAsyncRead = then;

  if (fdFamily == buffioFdFamily::file) {
    buffio::fiber::poller->push(header);
    buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
    buffio::fiber::poller->ping();
    return buffioRoutineStatus::none;
  };
  if (rwmask & BUFFIO_WRITE_READY) {
    this->unsetBit(BUFFIO_WRITE_READY);
    buffio::fiber::requestBatch->push(header);
    return buffioRoutineStatus::none;
  }

  pendingReadReq = header;
  return buffioRoutineStatus::none;
};

buffioRoutineStatus Fd::poll(int mask) {
  buffio::fiber::poller->pollMod(localfd.fd[0], this, mask);
  rwmask |= BUFFIO_FD_ACCEPT_READY;
  return buffioRoutineStatus::none;
};

void Fd::release() {
  auto family = this->fdFamily;
  this->fdFamily = buffioFdFamily::none;

  switch (family) {
  case buffioFdFamily::none:
    break;
  case buffioFdFamily::file:
    // TODO: add support for files
    break;
  case buffioFdFamily::pipe: {
    ::close(localfd.pipeFd[0]);
    ::close(localfd.pipeFd[1]);
    buffio::fiber::FdCount.fetch_add(-1, std::memory_order_acq_rel);
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
  buffio::fiber::FdCount.fetch_add(-1, std::memory_order_acq_rel);
};

void Fd::mountSocket(char *address, int socketfd, int portnumber) noexcept {
  buffio::fiber::poller->pollOp(socketfd, this);
  buffio::fiber::FdCount.fetch_add(1, std::memory_order_acq_rel);
  localfd.sock.socketFd = socketfd;
  localfd.sock.portnumber = portnumber;
  this->address = address;
  this->rwmask |= BUFFIO_FD_POLLED;
  return;
};

void Fd::mountPipe(int read, int write) noexcept {
  localfd.pipeFd[0] = read;
  localfd.pipeFd[1] = write;
  buffio::fiber::FdCount.fetch_add(2, std::memory_order_acq_rel);

  buffio::fiber::poller->pollOp(read, this, EPOLLIN | EPOLLET);
  buffio::fiber::poller->pollOp(write, this, EPOLLOUT | EPOLLET);
  this->rwmask |= BUFFIO_FD_POLLED;
};

void Fd::mountFile(int fd) {
  fdFamily = buffioFdFamily::file;
  localfd.fileFd = fd;
};
buffioHeader *Fd::getRawHeader() const {
  return buffio::fiber::headerPool->pop();
};
}; // namespace buffio
//
