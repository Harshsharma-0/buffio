#include "buffio/fd.hpp"
#include "buffio/actions.hpp"
#include "buffio/common.hpp"
#include "buffio/enum.hpp"
#include "buffio/fiber.hpp"

#include <cerrno>
#include <cstring>

namespace buffio {

int MakeFd::eventFd(buffio::Fd &fdCore, int initVal, int flags) {
  int fd = -1;
  if ((fd = eventfd(initVal, flags)) < 0)
    return (int)buffioErrorCode::fd;
  fdCore.mountEventFd(fd);

  return 0;
};

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
    break;
  case AF_INET:
    fdCore.fdFamily = buffioFdFamily::ipv4;
    portNumber = ntohs(((sockaddr_in *)&addr)->sin_port);
    break;
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

buffioHeader *Fd::waitReadReady() {
  if (readHeader.isFresh || rwmask & BUFFIO_READ_READY)
    return nullptr;

  readHeader.action = buffio::action::propBack;
  readHeader.aux = BUFFIO_READ_READY;
  readHeader.isFresh = true;
  pendingReadReq = &readHeader;
  return &readHeader;
};

buffioHeader *Fd::waitWriteReady() {
  if (writeHeader.isFresh || rwmask & BUFFIO_WRITE_READY)
    return nullptr;

  writeHeader.action = buffio::action::propBack;
  writeHeader.aux = BUFFIO_WRITE_READY;
  writeHeader.isFresh = true;
  pendingWriteReq = &writeHeader;
  return &writeHeader;
};

buffioRoutineStatus Fd::asyncConnect(onAsyncConnects then) {
  if (readHeader.isFresh)
    return buffioRoutineStatus::none;

  readHeader.action = buffio::action::asyncConnect;
  readHeader.isFresh = true;
  buffio::fiber::requestBatch->push(&readHeader);
  return buffioRoutineStatus::none;
};

inline void make_socked_header(buffioHeader &header, sockaddr *addr,
                               socklen_t len, buffioAction act) {

  header.data.socketaddr = addr;
  header.len.socklen = len;
  header.isFresh = true;
  header.opError = 0;
  header.action = act;
};
buffioHeader *Fd::waitAccept(struct sockaddr *addr, socklen_t len) {
  if (readHeader.isFresh)
    return nullptr;

  if (!(rwmask & BUFFIO_FD_ACCEPT_READY)) {
    buffio::fiber::poller->pollMod(localfd.fd[0], this, EPOLLIN | EPOLLET);
    rwmask |= BUFFIO_FD_ACCEPT_READY;
  };

  make_socked_header(readHeader, addr, len, buffio::action::waitAccept);

  if (rwmask & BUFFIO_READ_READY) {
    buffio::fiber::requestBatch->pushHead(&readHeader);
    return &readHeader;
  }
  buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
  pendingReadReq = &readHeader;
  return &readHeader;
};

buffioHeader *Fd::waitConnect(struct sockaddr *addr, socklen_t socklen) {

  if (writeHeader.isFresh)
    return nullptr;

  if (this->connect(addr, socklen) != -1)
    return nullptr;

  make_socked_header(writeHeader, addr, socklen, buffio::action::waitConnect);

  if (errno == EINPROGRESS) {
    buffio::fiber::requestBatch->push(&writeHeader);
    return &writeHeader;
  }

  writeHeader.opError = -1;
  return nullptr;
};

inline void make_read_write_header(buffioHeader &header, char *buffer,
                                   size_t len) {
  header.data.buffer = buffer;
  header.len.socklen = len;
  header.isFresh = true;
};
buffioHeader *Fd::waitRead(char *buffer, size_t len) {

  if (readHeader.isFresh)
    return nullptr;

  make_read_write_header(readHeader, buffer, len);

  if (fdFamily == buffioFdFamily::file) {
    readHeader.action = buffio::action::readFile;
    buffio::fiber::threadRequestBatch->push(&readHeader);
    return &readHeader;
  };

  readHeader.action = buffio::action::read;
  if (rwmask & BUFFIO_READ_READY) {
    buffio::fiber::requestBatch->push(&readHeader);
    return &readHeader;
  };

  pendingReadReq = &readHeader;
  buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
  return &readHeader;
};

buffioHeader *Fd::waitWrite(char *buffer, size_t len) {

  if (writeHeader.isFresh)
    return nullptr;

  make_read_write_header(writeHeader, buffer, len);

  if (fdFamily == buffioFdFamily::file) {
    writeHeader.action = buffio::action::writeFile;
    buffio::fiber::threadRequestBatch->push(&writeHeader);
    return &writeHeader;
  };

  writeHeader.action = buffio::action::write;
  buffio::fiber::requestBatch->push(&writeHeader);
  return &writeHeader;
};

buffioRoutineStatus Fd::asyncRead(char *buffer, size_t len, onAsyncReads then) {

  if (readHeader.isFresh)
    return buffioRoutineStatus::none;

  make_read_write_header(readHeader, buffer, len);
  readHeader.onAsyncDone.onAsyncRead = then;

  if (fdFamily == buffioFdFamily::file) {
    readHeader.action = buffio::action::asyncReadFile;
    buffio::fiber::threadRequestBatch->push(&readHeader);
    return buffioRoutineStatus::none;
  };

  readHeader.action = buffio::action::asyncRead;

  if (rwmask & BUFFIO_READ_READY) {
    buffio::fiber::requestBatch->push(&readHeader);
    return buffioRoutineStatus::none;
  }

  buffio::fiber::pendingReq.fetch_add(1, std::memory_order_acq_rel);
  pendingReadReq = &readHeader;
  return buffioRoutineStatus::none;
};

buffioRoutineStatus Fd::asyncWrite(char *buffer, size_t len,
                                   onAsyncWrites then) {

  if (writeHeader.isFresh)
    return buffioRoutineStatus::none;

  make_read_write_header(writeHeader, buffer, len);
  writeHeader.onAsyncDone.onAsyncRead = then;

  if (fdFamily == buffioFdFamily::file) {
    writeHeader.action = buffio::action::asyncWriteFile;
    buffio::fiber::threadRequestBatch->push(&writeHeader);
    return buffioRoutineStatus::none;
  };

  writeHeader.action = buffio::action::asyncWrite;
  buffio::fiber::requestBatch->push(&writeHeader);
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
    ::close(localfd.fd[0]);
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
  case buffioFdFamily::eventFd: {
    ::close(localfd.fd[0]);
  } break;
  };
  buffio::fiber::FdCount.fetch_add(-1, std::memory_order_acq_rel);
};

void Fd::mountSocket(char *address, int socketfd, int portnumber) noexcept {
  buffio::fiber::poller->pollOp(socketfd, this);
  buffio::fiber::FdCount.fetch_add(1, std::memory_order_acq_rel);

  localfd.sock.socketFd = socketfd;
  localfd.sock.portnumber = portnumber;

  readHeader.iFd = writeHeader.iFd = reserveHeader.iFd = socketfd;

  writeHeader.isFresh = readHeader.isFresh = false;

  this->address = address;
  this->rwmask |= BUFFIO_FD_POLLED;
  return;
};

void Fd::mountPipe(int read, int write) noexcept {
  localfd.pipeFd[0] = read;
  localfd.pipeFd[1] = write;

  readHeader.iFd = read;
  writeHeader.iFd = write;
  writeHeader.isFresh = readHeader.isFresh = false;

  buffio::fiber::FdCount.fetch_add(2, std::memory_order_acq_rel);
  buffio::fiber::poller->pollOp(read, this, EPOLLIN | EPOLLET);
  buffio::fiber::poller->pollOp(write, this, EPOLLOUT | EPOLLET);
  this->rwmask |= BUFFIO_FD_POLLED;
};

void Fd::mountFile(int fd) {
  fdFamily = buffioFdFamily::file;
  localfd.fileFd = fd;
  readHeader.iFd = writeHeader.iFd = reserveHeader.iFd = fd;
  readHeader.isFresh = writeHeader.isFresh = false;
};
void Fd::mountEventFd(int fd) {

  buffio::fiber::poller->pollOp(fd, this);
  fdFamily = buffioFdFamily::eventFd;
  localfd.fileFd = fd;
  readHeader.iFd = writeHeader.iFd = reserveHeader.iFd = fd;
  readHeader.isFresh = writeHeader.isFresh = false;
  buffio::fiber::FdCount.fetch_add(1, std::memory_order_acq_rel);
};
void Fd::takeEventReadAction() {
  if (pendingReadReq == nullptr) {
    rwmask &= BUFFIO_READ_READY;
    return;
  };
  buffio::fiber::pendingReq.fetch_add(-1, std::memory_order_acq_rel);
  buffio::fiber::requestBatch->push(pendingReadReq);
  pendingReadReq = nullptr;
};

void Fd::takeEventWriteAction() {
  if (pendingWriteReq == nullptr) {
    rwmask &= BUFFIO_WRITE_READY;
    return;
  };

  buffio::fiber::pendingReq.fetch_add(-1, std::memory_order_acq_rel);
  buffio::fiber::requestBatch->push(pendingWriteReq);
  pendingWriteReq = nullptr;
};

}; // namespace buffio
