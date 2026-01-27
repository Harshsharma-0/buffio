#ifndef BUFFIO_SOCK_BROKER
#define BUFFIO_SOCK_BROKER
/*
 * Error codes range reserved for buffiosockbroker
 *
 *  [1000 - 2000]
 *  1000 <= errorcode <= 2000
 *
 *
 *
 */

#if !defined(BUFFIO_IMPLEMENTATION)
// #include "buffioenum.hpp"
#include "buffiofd.hpp"
#include "buffiolfqueue.hpp"
#include "buffiopromise.hpp"
#include "buffiothread.hpp"
#include <memory>
#endif

#include <sys/epoll.h>

#define broker_cfg_nodiscard_msg                                               \
  "The value returned by this function must not be discarded"

typedef struct buffioSockBrokerConfig {
  sockBrokerConfigFlags sockBrokerFlags;
  int workerNum;   // number of the workers;
  int expectedFds; // number of the expected fd to consume across
  int queueSize;   // mast be of power of 2, size is calculated via
                   // 2^queuesize;
  sockBrokerPollerType pollerType;     // type of I/O architecture to use;
  sockBrokerWorkerPolicy WorkerPolicy; // policy of for the workers

} buffioSockBrokerConfig;

// opcodes comes from the buffiosock.hpp

enum class sbEpollState : uint32_t {
  none = 0,
  emptyOk = 1u,
  entryOk = 1u << 1,
  consumeOk = 1u << 2,
  worksOk = 1u << 3,
  eventsOk = 1u << 4,
  eventSizeOk = 1u << 5,
  fdOk = 1u << 6,
  threadOk = 1u << 7,
  threadRunOk = 1u << 8
};

#define operator_for sbEpollState
#include "buffiooperator.hpp"

constexpr sbEpollState sbEpollStateOk =
    sbEpollState::emptyOk | sbEpollState::entryOk | sbEpollState::consumeOk |
    sbEpollState::worksOk | sbEpollState::eventsOk | sbEpollState::eventSizeOk |
    sbEpollState::fdOk | sbEpollState::threadOk | sbEpollState::threadRunOk;

using buffioSockBrokerQueue = buffiolfqueue<buffioFdViewWeak>;

#define bf_io_io_uringfd_ok (1 << 9)
#define bf_io_uconsumed_ok (1 << 1)
#define bf_io_uenterentry_ok (1 << 2)
#define bf_io_uconsumeentry_ok (1 << 3)
#define bf_io_usq_ok (1 << 4)
#define bf_io_ucq_ok (1 << 5)

// not defined yet
#define bf_io_ok 0
class buffioSockBroker {

  // the main thread worker code inlined with the code to support hybrid arch
  // only call to register fd for read write.

  static inline int buffio_ep_thread_poll(int fd,
                                          buffioSockBrokerQueue *ep_entry,
                                          buffioSockBrokerQueue *ep_works,
                                          buffioSockBrokerQueue *ep_consume) {

    size_t evnt_size = 1024;
    struct epoll_event evnt[1024];

    return 0;
  }

  static inline int buffio_ep_thread_worker(void *data) { return 0; }

  static int buffio_epoll_poller_modular(void *data) { return 0; }

  __attribute__((used)) static int buffio_epoll_worker_modular(void *data) {
    return 0;
  }
  __attribute__((used)) static int buffio_epoll_monolithic(void *data) {

    return 0;
  }
  __attribute__((used)) static int buffio_iouring_poller(void *data) {
    return 0;
  }

public:
  buffioSockBroker()
      : epollFd(-1), epollTotalFd(0), ioUringFd(-1), epollThreads(nullptr) {

    sockBrokerConfig.sockBrokerFlags = sockBrokerConfigFlags::none;
    sockBrokerState = buffioSockBrokerState::inActive;
    epollConfigured = sbEpollState::none;
  };

  ~buffioSockBroker() {
    switch (sockBrokerState) {
    case buffioSockBrokerState::epollRunning:
    case buffioSockBrokerState::epoll:
      shutpoll();
      break;
    }
    return;
  }

  [[nodiscard(broker_cfg_nodiscard_msg)]]
  constexpr buffioErrorCode configure(struct buffioSockBrokerConfig &config) {

    if (config.workerNum > 0 && config.expectedFds > 0 &&
        config.sockBrokerFlags == sockBrokerConfigFlags::none) {

      if (config.queueSize >= BUFFIO_RING_MIN &&
          config.queueSize <= buffioatomix_max_order) {

        if ((config.workerNum + 1) >= (1 << config.queueSize))
          return buffioErrorCode::workerNum;

        sockBrokerConfig = config;
        sockBrokerConfig.sockBrokerFlags = sockBrokerConfigOk;

        return buffioErrorCode::none;
      }
      return buffioErrorCode::queueSize;
    }

    return buffioErrorCode::workerNum;
  }

  buffioErrorCode init() {
    if (sockBrokerState != buffioSockBrokerState::inActive)
      return buffioErrorCode::unknown;

    if (sockBrokerConfig.sockBrokerFlags == sockBrokerConfigOk) {
      return buffioErrorCode::unknown;
    }
    return buffioErrorCode::unknown;
  }

  int pushreq() { return 0; }
  int popreq() { return 0; }

  bool running() {
    if (sockBrokerState != buffioSockBrokerState::inActive &&
        sockBrokerState != buffioSockBrokerState::error)
      return true;

    return false;
  };

  buffioSockBroker(const buffioSockBroker &) = delete;
  buffioSockBroker &operator=(const buffioSockBroker &) = delete;

  buffioErrorCode pollFd(struct buffioFdReq_add *entry) {
    if (sockBrokerState != buffioSockBrokerState::inActive &&
        epollConfigured == sbEpollStateOk) {
      switch (entry->opcode) {
      case buffio_fd_opcode::start_poll:
        break;
      case buffio_fd_opcode::end_poll:
        break;
      }
      return buffioErrorCode::none;
    };
    return buffioErrorCode::epollInstance;
  };

private:
  buffioErrorCode configureEpoll() {
    // code below here is used for epoll instance,

    if (epollConfigured == sbEpollStateOk)
      return buffioErrorCode::occupied;

    int ep_fd_tmp = createEpollInstance(&epollFd);
    if (ep_fd_tmp < 0)
      return buffioErrorCode::fd;

    epollConfigured |= sbEpollState::fdOk;
    epollConfigured |= sbEpollState::entryOk;
    epollConfigured |= sbEpollState::worksOk;
    epollConfigured |= sbEpollState::consumeOk;
    epollConfigured |= sbEpollState::eventsOk;
    epollConfigured |= sbEpollState::eventSizeOk;
    epollConfigured |= sbEpollState::threadRunOk | sbEpollState::emptyOk;
    epollTotalFd = 0;

    sockBrokerState = buffioSockBrokerState::epoll;
    return buffioErrorCode::none;
  }

  void shutpoll() {

    sbEpollState mask = epollConfigured;
    if (ep_has_flag(mask, sbEpollState::fdOk)) {
      close(epollFd);
      mask &= ~(sbEpollState::fdOk); // unsetting the mask;
    }
    if (ep_has_flag(mask, sbEpollState::entryOk))
      mask &= ~(sbEpollState::entryOk);

    if (ep_has_flag(mask, sbEpollState::worksOk))
      mask &= ~(sbEpollState::worksOk);

    if (ep_has_flag(mask, sbEpollState::consumeOk))
      mask &= ~(sbEpollState::consumeOk);

    mask &= ~(sbEpollState::eventsOk);

    if (ep_has_flag(mask, sbEpollState::threadOk)) {
      if (ep_has_flag(mask, sbEpollState::threadRunOk)) {
        mask &= ~(sbEpollState::threadRunOk);
      }
      delete[] epollThreads;
      epollThreads = nullptr;
      mask &= ~(sbEpollState::threadOk);
    }
    epollConfigured = sbEpollState::none;
    sockBrokerState = buffioSockBrokerState::inActive;
  };

  [[nodiscard]] int createEpollInstance(int *fdr) {
    int fd = epoll_create1(EPOLL_CLOEXEC);
    if (fd >= 0) {
      *fdr = fd;
      return 0;
    }
    return -1;
  }

  buffioThread sockBrokerThreadPool;
  buffioSockBrokerConfig sockBrokerConfig;
  buffioSockBrokerState sockBrokerState;
  buffioSockBrokerQueue epollentry;
  buffioSockBrokerQueue epollWorks;
  buffioSockBrokerQueue epollConsume;
  pthread_t *epollThreads;
  size_t epollTotalFd;
  sbEpollState epollConfigured;
  std::atomic<int> epollEmpty;
  int epollFd;
  int ioUringFd;
};

#endif
