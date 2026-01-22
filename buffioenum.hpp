#ifndef BUFFIO_ENUM
#define BUFFIO_ENUM
#include <cstdint>

enum class buffio_routine_status : uint32_t {
  waiting = 21,
  executing = 22,
  yield = 23,
  error = 24,
  paused = 25,
  push_task = 26,
  done = 27,
  unhandled_exception = 28,
  waiting_io = 29,
};

//[reserved enum 31 - 49]

enum class buffio_eventloop_type : uint32_t {
  synced = 50,
  async = 51,
};

//[reservec enum 60 - 70]

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

enum class sockBrokerPollerType : uint32_t {
  nono = 91,
  monolithic = 92,
  modular = 93,
  io_uring = 94
};

enum class sockBrokerWorkerPolicy : uint32_t {
  consume = 101,
  wait = 102,
  hybrid = 103
};

enum class buffio_fd_opcode : uint32_t {
  read = 150,
  write = 151,
  start_poll = 152,
  end_poll = 153,
  page_read = 154,
  page_write = 155
};

enum class sockBrokerConfigFlags : uint32_t {
  none = 0,
  workerNumOk = 1u << 0,
  expectedFdsOk = 1u << 1,
  queueSizeOk = 1u << 2,
  pollerTypeOk = 1u << 3,
  workerPolicyOk = 1u << 4
};

#define operator_for sockBrokerConfigFlags
#include "buffiooperator.hpp"

constexpr sockBrokerConfigFlags sockBrokerConfigOk =
    sockBrokerConfigFlags::workerNumOk | sockBrokerConfigFlags::expectedFdsOk |
    sockBrokerConfigFlags::queueSizeOk | sockBrokerConfigFlags::pollerTypeOk |
    sockBrokerConfigFlags::workerPolicyOk;

#endif
