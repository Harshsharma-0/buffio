#include "buffio/actions.hpp"
#include "buffio/promise.hpp"
#include <cerrno>
#include <iostream>
namespace buffio {

action::xeturn action::propBack(buffioHeader *header) {
  header->isFresh = false;
  header->fd->unsetBit(header->aux);
  return header->routine;
};
action::xeturn action::clampThread(buffioHeader *header) {
  header->routine.resume();
  return header->routine;
};

action::xeturn action::read(buffioHeader *header) {

  errno = 0;
  ssize_t buffiolen = 1;
  header->opError = 0;
  int fd = header->iFd;
  ssize_t reserved = header->len.len;
  char *cursor = header->data.buffer;
  char *buffer = header->data.buffer;

  while (buffiolen > 0) {
    buffiolen = ::read(fd, cursor, reserved);
    if (buffiolen > 0) {
      reserved -= buffiolen;
      if (reserved <= 0)
        break;
      cursor = buffer + buffiolen;
    };
  };

  header->len.len = cursor - buffer;
  header->isFresh = false;

  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    header->fd->unsetBit(BUFFIO_READ_READY);
    return header->routine;
  };
  return header->routine;
};

action::xeturn action::write(buffioHeader *header) {
  errno = 0;

  ssize_t buffiolen = 1;
  header->opError = 0;
  int fd = header->iFd;
  ssize_t reserved = header->len.len;
  char *cursor = header->data.buffer;
  char *buffer = header->data.buffer;
  while (buffiolen > 0) {
    buffiolen = ::write(fd, cursor, reserved);
    if (buffiolen > 0) {
      reserved -= buffiolen;
      if (reserved <= 0)
        break;
      cursor = buffer + buffiolen;
    };
  };

  header->len.len = cursor - buffer;
  header->isFresh = false;

  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    header->fd->unsetBit(BUFFIO_WRITE_READY);
    return header->routine;
  };
  return header->routine;
};
action::xeturn action::readFile(buffioHeader *header) {
  int rlen = 0;
  rlen = ::read(header->iFd, header->data.buffer, header->len.len);
  header->opError = errno;
  header->len.len = rlen;
  header->isFresh = false;

  return header->routine;
};
action::xeturn action::writeFile(buffioHeader *header) {
  int rlen = 0;
  rlen = ::write(header->iFd, header->data.buffer, header->len.len);
  header->opError = errno;
  header->len.len = rlen;
  header->isFresh = false;

  return header->routine;
};

action::xeturn action::asyncConnect(buffioHeader *header) {

  int code = -1;
  socklen_t len = sizeof(int);
  buffio::Fd *fd = header->fd;
  action::xeturn handle;

  if (::getsockopt(header->iFd, SOL_SOCKET, SO_ERROR, &code, &len) != 0)
    fd = nullptr;

  handle = header->onAsyncDone.onAsyncConnect(code, fd, header->data.socketaddr)
               .get();
  header->isFresh = false;

  return handle;
};
action::xeturn action::waitConnect(buffioHeader *header) {

  int error = -1;
  socklen_t len = sizeof(int);

  if (getsockopt(header->iFd, SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
    header->opError = -1;
    header->isFresh = false;
    return header->routine;
  };

  if (error != 0) {
    ::connect(header->iFd, header->data.socketaddr, header->len.socklen);
    if (errno != EINPROGRESS) {
      header->opError = -1;
    };

    header->isFresh = false;
    return header->routine;
  }

  header->isFresh = false;
  return header->routine;
};

action::xeturn action::asyncAccept(buffioHeader *header) {

  sockaddr_un addr = {0};
  header->data.socketaddr = (sockaddr *)&addr;
  header->len.socklen = sizeof(addr);
  action::waitAccept(header);

  return header->onAsyncDone
      .asyncAcceptlocal(header->aux, addr, header->len.socklen)
      .get();
};
action::xeturn action::asyncAcceptIpv4(buffioHeader *header) {
  sockaddr_in addr = {0};
  header->data.socketaddr = (sockaddr *)&addr;
  header->len.socklen = sizeof(sockaddr_in);

  action::waitAccept(header);
  return header->onAsyncDone
      .asyncAcceptin(header->aux, addr, header->len.socklen)
      .get();
};
action::xeturn action::asyncAcceptIpv6(buffioHeader *header) {
  sockaddr_in6 addr = {0};
  header->data.socketaddr = (sockaddr *)&addr;
  header->len.socklen = sizeof(sockaddr_in6);
  action::waitAccept(header);
  return header->onAsyncDone
      .asyncAcceptin6(header->aux, addr, header->len.socklen)
      .get();
};

action::xeturn action::waitAccept(buffioHeader *header) {

  int afd =
      ::accept(header->iFd, header->data.socketaddr, &header->len.socklen);
  header->opError = errno;
  header->aux = afd;
  return header->routine;
}

action::xeturn action::asyncRead(buffioHeader *header) {
  action::read(header);
  auto handle = header->onAsyncDone
                    .onAsyncRead(header->opError, header->data.buffer,
                                 header->len.len, header->fd)
                    .get();
  header->isFresh = false;
  return handle;
};
action::xeturn action::asyncWrite(buffioHeader *header) {
  action::write(header);
  auto handle = header->onAsyncDone
                    .onAsyncWrite(header->opError, header->data.buffer,
                                  header->len.len, header->fd)
                    .get();
  header->isFresh = false;
  return handle;
};

action::xeturn action::asyncReadFile(buffioHeader *header) {
  action::readFile(header);
  auto handle = header->onAsyncDone
                    .onAsyncRead(header->opError, header->data.buffer,
                                 header->len.len, header->fd)
                    .get();
  header->isFresh = false;
  return handle;
};
action::xeturn action::asyncWriteFile(buffioHeader *header) {
  action::writeFile(header);
  auto handle = header->onAsyncDone
                    .onAsyncWrite(header->opError, header->data.buffer,
                                  header->len.len, header->fd)
                    .get();
  header->isFresh = false;
  return handle;
};
}; // namespace buffio
