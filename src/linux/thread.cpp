#include "buffio/thread.hpp"
#include "buffio/ecode.hpp"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <syscall.h>
#include <unistd.h>

int buffio::semaphore::create(size_t initialValue) {
  if (sem_init(&lsem, 0, initialValue) == 0)
    return 0;

  return B_ESEMCREATE;
};

int buffio::semaphore::post() {
  if (sem_post(&lsem) != 0)
    return buffio::error_from_os(errno);
  return 0;
};

int buffio::semaphore::wait() {
  if (sem_wait(&lsem) != 0) {
    return buffio::error_from_os(errno);
  };
  return 0;
};

void buffio::semaphore::destroy() { sem_destroy(&lsem); };

int buffio::thread::run(buffio::threadFuncSig start, void *args) {

  size_t stackSize = 5 * 1024 * 1024;
  this->routine = start;
  this->args = args;

  auto threadMainRoutine = [](void *args) -> void * {
    buffio::thread *instance = static_cast<buffio::thread *>(args);
    instance->routine(instance->args);
    return nullptr;
  };

  pthread_attr_t attribute;
  pthread_attr_init(&attribute);
  pthread_attr_setstacksize(&attribute, stackSize);
  void *pArgs = static_cast<void *>(this);

  if (pthread_create(&this->handle, &attribute, threadMainRoutine, pArgs) !=
      0) {

    if (EAGAIN == errno) {
      return B_ETHREADNOMEM;
    };
    if (EINVAL == errno)
      return B_ETHREADARGS;

    return B_ETHREAD;
  };

  pthread_attr_destroy(&attribute);

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

int buffio::thread::join(uint32_t timout) {

  void *val = nullptr;

  int res = pthread_tryjoin_np(handle, &val);
  if (res == 0)
    return 0;

  struct timespec tv;
  unsigned long int ms = static_cast<unsigned long int>(timout);

  if (clock_gettime(CLOCK_REALTIME, &tv) == -1) {
    return buffio::error_from_os(errno);
  }

  // TODO: verify if nsec affect sleep
  tv.tv_nsec += (ms % 1000) * 1000000L;
  tv.tv_sec += ms / 1000;

  if (pthread_timedjoin_np(handle, NULL, &tv) == -1) {
    return buffio::error_from_os(errno);
  }

  return 0;
};
