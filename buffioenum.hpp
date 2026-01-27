#ifndef BUFFIO_ENUM
#define BUFFIO_ENUM
#include <cstdint>

enum class buffioRoutineStatus : uint32_t {
  waiting = 21,
  executing = 22,
  yield = 23,
  error = 24,
  paused = 25,
  pushTask = 26,
  done = 27,
  unhandledException = 28,
  waitingsFd = 29,
};

//[reservec enum 31 - 70]

enum class buffioSockBrokerState : uint32_t {
  inActive = 71,
  error = 72,
  epoll = 73,
  ioUring = 74,
  epollRunning = 75,
  ioUringRunning = 76
};

enum class buffioThreadStatus : uint32_t {
  none = 81,
  running = 82,
  killed = 83,
  done = 84,
  error = 85,
  configOk = 86,
};

enum class buffio_fd_opcode : uint32_t {
  read = 150,
  write = 151,
  start_poll = 152,
  end_poll = 153,
  page_read = 154,
  page_write = 155
};

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
  X(socket, -14, "")                                                           \
  X(bind, -15, "")                                                             \
  X(open, -16, "")                                                             \
  X(filePath, -17, "")                                                         \
  X(socketAddress, -18, "")                                                    \
  X(portnumber, 19, "")                                                        \
  X(pipe, -20, "")                                                             \
  X(fifo, -21, "")                                                             \
  X(fifoPath, -22, "")                                                         \
  X(family, -23, "")                                                           \
  X(fcntl, -24, "")                                                            \
  X(makeShared, -25, "")                                                       \
  X(makeUnique, -26, "")

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

#endif
