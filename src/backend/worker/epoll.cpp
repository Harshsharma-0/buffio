#include "buffio/worker.hpp"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>


void buffio::Worker::signalLoop(buffio_fd fd,LoopStatusCode code){
      uint64_t event =
          static_cast<uint64_t>(code);
      ssize_t ret = write(fd, &event, sizeof(event));
      (void)ret;
};


int buffio::Worker::init_poller(unsigned int order) {

  int epfd = epoll_create1(EPOLL_CLOEXEC);
  if (epfd < 0)
    return buffio::error_from_os(errno);

  int evntfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (evntfd < 0) {
    close(epfd);
    return buffio::error_from_os(errno);
  };

  struct epoll_event evnt;
  evnt.events = EPOLLIN;
  evnt.data.fd = evntfd;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, evntfd, &evnt) < 0) {
    close(epfd);
    close(evntfd);
    return buffio::error_from_os(errno);
  };

  state.event.evfd = epfd;
  state.event.sigfd = evntfd;
  return 0;
};


int buffio::Worker::wait_event() {

  int evfd = state.event.evfd;
  int sigfd = state.event.sigfd;
  constexpr int event_size = 1024;
  struct epoll_event events[1024];

  int timeout = state.task_queue.empty() && state.io.pending != 0 ? -1 : 0;
  if (timeout < 0)
    state.control.store(buffio::LoopStatusCode::inactive,
                        std::memory_order_release);
  int count = epoll_wait(evfd, events, event_size, timeout);
  if (timeout < 0)
    state.control.store(buffio::LoopStatusCode::active,
                        std::memory_order_release);

  if (count < 0 && errno != EINTR)
    return buffio::error_from_os(errno);

  struct epoll_event *evnt = nullptr;
  for (int i = 0; i < count; i++) {
    evnt = (events + i);

    if (evnt->data.fd == sigfd) {
      uint64_t value;

      while (read(sigfd, &value, sizeof(value)) == sizeof(value)) {
        // Drain/coalesce notifications.
      }
      flush_io_completed(64);

      continue;
    }

    buffio::OpState *op = static_cast<buffio::OpState *>(evnt->data.ptr);

    /* perform socket read/write */
  };

  return 0;
};

static void sleep_ms(unsigned long int ms) {

  struct timespec ts;
  struct timespec rem;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;

  if (nanosleep(&ts, &rem) < 0) {
    if (errno == EINTR)
      nanosleep(&rem, &ts);
  };
};
