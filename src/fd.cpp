#include "buffio/fd.hpp"

namespace buffio {

int MakeFd::createSocket(buffio::Fd &fdCore, struct sockaddr *lsocket,
                         const char *address, int portNumber,
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

  char ip[INET_ADDRSTRLEN];
  sockaddr_in *tmp = (sockaddr_in *)&addr;
  inet_ntop(AF_INET, &tmp->sin_addr, ip, sizeof(ip));

  if (::bind(socketFd, reinterpret_cast<sockaddr *>(&addr), addrLen) != 0) {
    ::close(socketFd);

    return (int)buffioErrorCode::bind;
  }

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
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1)
    return (int)buffioErrorCode::fcntl;

  return (int)buffioErrorCode::none;
}
}; // namespace buffio

namespace buffio {

Fd::~Fd() { release(); }

buffioRoutineStatus Fd::asyncConnect(onAsyncConnects then) {
  reserveHeader.opCode = buffioOpCode::asyncConnect;
  reserveHeader.fd = this;
  reserveHeader.reqToken.fd = localfd.fd[0];
  return buffioRoutineStatus::none;
};

buffioRoutineStatus Fd::waitAccept(struct sockaddr *addr, socklen_t len) {
  reserveHeader.opCode = buffioOpCode::waitAccept;
  reserveHeader.fd = this;
  reserveHeader.reqToken.fd = localfd.fd[0];
  reserveHeader.data.socketaddr = addr;
  reserveHeader.len.socklen = len;
  return buffioRoutineStatus::waitingFd;
};
buffioRoutineStatus Fd::waitConnect() {
  reserveHeader.opCode = buffioOpCode::waitConnect;
  reserveHeader.unsetBit = BUFFIO_READ_READY;
  reserveHeader.fd = this;
  reserveHeader.reqToken.fd = localfd.fd[0];
  return buffioRoutineStatus::waitingFd;
};

buffioRoutineStatus Fd::waitRead(char *buffer, size_t len) {
  reserveHeader.opCode = fdFamily == buffioFdFamily::file
                             ? buffioOpCode::readFile
                             : buffioOpCode::read;
  reserveHeader.fd = this;
  reserveHeader.unsetBit = BUFFIO_READ_READY;
  reserveHeader.data.buffer = buffer;
  reserveHeader.len.len = len;
  reserveHeader.reqToken.fd = localfd.fd[0];
  return buffioRoutineStatus::waitingFd;
};
buffioRoutineStatus Fd::waitWrite(char *buffer, size_t len) {
  reserveHeader.opCode = fdFamily == buffioFdFamily::file
                             ? buffioOpCode::writeFile
                             : buffioOpCode::write;

  reserveHeader.fd = this;
  reserveHeader.data.buffer = buffer;
  reserveHeader.unsetBit = BUFFIO_WRITE_READY;
  reserveHeader.len.len = len;
  reserveHeader.reqToken.fd = localfd.fd[0];

  return buffioRoutineStatus::waitingFd;
};

buffioRoutineStatus Fd::asyncRead(char *buffer, size_t len, onAsyncReads then) {
  auto header = getRawHeader();

  header->opCode = fdFamily == buffioFdFamily::file
                       ? buffioOpCode::asyncReadFile
                       : buffioOpCode::asyncRead;

  header->rwtype = buffioReadWriteType::read;
  header->fd = this;
  header->unsetBit = BUFFIO_READ_READY;
  header->reqToken.fd = localfd.fd[0];
  header->data.buffer = buffer;
  header->len.len = len;
  header->onAsyncDone.onAsyncRead = then;

  if (fdFamily == buffioFdFamily::file) {
    buffio::fiber::poller->push(header);
    return buffioRoutineStatus::none;
  };

  if (rwmask & BUFFIO_READ_READY) {
    buffio::fiber::requestBatch->push(header);

    return buffioRoutineStatus::none;
  }
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
    return buffioRoutineStatus::none;
  };
  if (rwmask & BUFFIO_WRITE_READY) {
    buffio::fiber::requestBatch->push(header);

    return buffioRoutineStatus::none;
  }

  pendingReadReq = header;
  return buffioRoutineStatus::none;
};

buffioRoutineStatus Fd::poll(int mask) { return buffioRoutineStatus::none; };

void Fd::release() {
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

void Fd::mountSocket(char *address, int socketfd, int portnumber) noexcept {
  buffio::fiber::poller->pollOp(socketfd, this);
  localfd.sock.socketFd = socketfd;
  localfd.sock.portnumber = portnumber;
  this->address = address;
  this->rwmask |= BUFFIO_FD_POLLED;
  return;
};

void Fd::mountPipe(int read, int write) noexcept {
  localfd.pipeFd[0] = read;
  localfd.pipeFd[1] = write;
  buffio::fiber::poller->pollOp(read, this, EPOLLIN | EPOLLET);
  buffio::fiber::poller->pollOp(write, this, EPOLLOUT | EPOLLET);
  this->rwmask |= BUFFIO_FD_POLLED;
};

buffioHeader *Fd::getRawHeader() const {
  return buffio::fiber::headerPool->pop();
};
}; // namespace buffio
//
