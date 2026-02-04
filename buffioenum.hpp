#ifndef BUFFIO_ENUM
#define BUFFIO_ENUM
#include <cstdint>

enum class buffioRoutineStatus : uint32_t {
  fresh = 20,
  waiting = 21,
  executing = 22,
  yield = 23,
  error = 24,
  paused = 25,
  pushTask = 26,
  done = 27,
  unhandledException = 28,
  waitingFd = 29,
  zombie = 30,
  wakeParent = 31,
  waitingOp = 32,
  waitingTimer = 33,

};

enum class buffioOrigin : uint32_t {
  none = 0,
  routine = 1,
  pool = 2,
};

//[reserved enum 31 - 70]

enum class buffioSockBrokerState : uint32_t {
  active = 71,
  none = 72,
  error = 73,
};

enum class buffioThreadStatus : uint32_t {
  none = 81,
  running = 82,
  killed = 83,
  done = 84,
  error = 85,
  configOk = 86,
};

enum class buffioOpCode : uint8_t {
  /*
   * none: define no operation
   */
  none = 0,
  /*
   * read and write opcode to used for pipe,fifo,and tcp socket
   */

  read = 1,
  write = 2,
  /* opcode to signal shutdowm of the eventloop
   */
  abort = 3,

  /* opcode to write to file so it can be
   * relayed to the worked thread.
   */

  readFile = 4,
  writeFile = 5,

  /* required as we want to also give back the address from
   * where the message use received.
   */
  readUdp = 6,
  writeUdp = 7,

  /*
   * request to connect to a specific socket
   */
  connect = 8,

  /*
   * opCode to release the header memory back to the memoryPool
   */
  release = 9,
  /*
   * use this opCode to add the fd to for polling;
   */
  poll = 10,
  /*
   *
   * syncFd : opCode to sync the fd class to the current event loop
   * and configure the header request pool in the fd class so it can
   * be used, any fd, must be first synchronised with the event loop
   * before any opration on it, as it can fail, if not done.
   * [deprecated];
   */
  syncFd = 11,
  /*
   * rmPoll : opCode used to remove a fd from the poller, and user owned fd,
   * created by the used locally, not pulled from the eventloop,
   * must be unpolled. after completion.
   *
   */
  rmPoll = 12,
  /*
   *
   *
   */
  asyncConnect = 13,
  asyncAcceptlocal = 14,
  asyncAcceptin = 15,
  asyncAcceptin6 = 16,

  asyncRead = 17,
  asyncWrite = 18,

  waitAccept = 19,
  waitConnect = 20,

  done = 21,
  dequeueThread = 22,
};
enum class buffioReadWriteType : uint8_t {

  /*
   * read/write types maps normal linux read/write
   * system call that can be used for any fd.
   *
   */
  read = 0,
  write = 1,

  /* recv/recvFrom maps to linux recv/recvfrom
   * system call, the and must only be used for,
   * sockets, not on files,
   *
   * difference between revc and recvfrom, is that
   * recvfrom takes struct sockaddr as arguement,and
   * it's len, so, from where the data was, received from
   * can be known.
   *
   */

  recv = 2,
  recvfrom = 3,

  /* send/sentto maps to linux send/sendto system call,
   * send/sendto is used to write data, to sockets,
   * and sendto can be used when sending msg to specific,
   * address is required,
   * and takes the struct sockaddr as arguement, and the
   * len of the socket aslo.
   */

  send = 4,
  sendto = 5,

  rwEnd = 6,
};

/*
 * BUFFIO_ERROR_LIST: x-macro defination for error codes mapping
 *     and string for the error code.
 */

#define BUFFIO_ERROR_LIST                                                      \
  X(none, 0, "buffio no error")                                                \
  X(workerNum, -1,                                                             \
    "Configuration worker numnber not configured properly,make "               \
    "sure the worker num is smaller than queue size")                          \
  X(expectedFds, -2, "Expected Fds error in struct field")                     \
  X(queueSize, -3,                                                             \
    "Queue Size must be in range [buffio_queue_min,buffio_queue_max], "        \
    "queuesize is calculated via 2^(queueorder).")                             \
  X(pollerType, -4, "poller type requested is not implemented")                \
  X(workerPolicy, -5,                                                          \
    "invalid worker policy,make sure right worker policy is given")            \
  X(unknown, -6, "cannot track error type, unknown error")                     \
  X(epollInstance, -7,                                                         \
    "failed to create epoll instance, poller not configured")                  \
  X(epollEmptyFlag, -8, "failed to allocate Epoll empty flag")                 \
  X(epollEntry, -9, "epoll entryQueue configuration failed")                   \
  X(epollConsume, -10, "epoll consumeQueue configuration failed")              \
  X(epollWorks, -11, "epoll workQueue configuration failed")                   \
  X(occupied, -12, "instance is currently occupied")                           \
  X(fd, -13, "failed to create Fd")                                            \
  X(socket, -14, "failed to create socket")                                    \
  X(bind, -15, "failed to bind socket to address")                             \
  X(open, -16, "failed to open file")                                          \
  X(filePath, -17, "error filepath not valid")                                 \
  X(socketAddress, -18, "sockket address not valid")                           \
  X(portnumber, 19, "port number not valid")                                   \
  X(pipe, -20, "faild to create pipe")                                         \
  X(fifo, -21, "failed to create fifo")                                        \
  X(fifoPath, -22, "fifo path is not valid")                                   \
  X(family, -23, "buffioFd protocl family not supported")                      \
  X(fcntl, -24, "failed to set fd flags using fnctl,error")                    \
  X(makeShared, -25, "failed to create a shared ptr")                          \
  X(makeUnique, -26, "failed to create a unique ptr")                          \
  X(protocol, -27, "error invalid protocol number")                            \
  X(protocolString, -28, "error open, no protocol string")

#define X(ERROR_ENUM, ERROR_CODE, MESSAGE) ERROR_ENUM = ERROR_CODE,
enum class buffioErrorCode : int { BUFFIO_ERROR_LIST };
#undef X

constexpr const char *buffioStrError(buffioErrorCode code) {

  switch (code) {
#define X(ERROR_NUM, ERROR_CODE, MESSAGE)                                      \
  case buffioErrorCode::ERROR_NUM:                                             \
    return MESSAGE;                                                            \
    break;

    BUFFIO_ERROR_LIST
#undef X
  };
  return "invalid error code";
};
constexpr const char *buffioStrErrno(int code) {
  return buffioStrError((buffioErrorCode)code);
};

#endif
