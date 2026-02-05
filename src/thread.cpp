#include "buffio/thread.hpp"

namespace buffio {
thread::thread() {
  threads = nullptr;
  numThreads = 0;
  mutexEnabled = false;
  buffioMutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutexattr_t mutexAttr;

  if (pthread_mutexattr_init(&mutexAttr) != 0)
    return;
  if (pthread_mutex_init(&buffioMutex, &mutexAttr) != 0)
    return;
  pthread_mutexattr_destroy(&mutexAttr);
  mutexEnabled = true;
};

void thread::free() {

  if (mutexEnabled == false)
    return;

  ::pthread_mutex_lock(&buffioMutex);
  struct threadinternal *tmp = nullptr;
  for (auto *loop = threads; loop != nullptr;) {
    if (loop->name != nullptr)
      delete[] loop->name;
    ::free(loop->stack);
    tmp = loop;
    loop = loop->next;
    delete tmp;
  }
  threads = nullptr;
  ::pthread_mutex_unlock(&buffioMutex);
  pthread_mutex_destroy(&buffioMutex);
  return;
};
int thread::run(const char *name, int (*func)(void *), void *data,
                size_t stackSize) {

  assert(this != nullptr);
  if (stackSize < buffio::thread::S1KB || func == nullptr ||
      mutexEnabled == false)
    return -1;

  struct threadinternal *tmpThr = nullptr;
  try {
    tmpThr = new struct threadinternal;
  } catch (std::exception &e) {
    return -1;
  };

  ::memset(tmpThr, '\0', sizeof(struct threadinternal));
  tmpThr->status = buffioThreadStatus::configOk;

  if (::pthread_attr_init(&tmpThr->attr) != 0) {
    delete tmpThr;
    return -1;
  }

  tmpThr->resource = data;
  tmpThr->name = nullptr;
  tmpThr->stackSize = stackSize;
  tmpThr->numThreads = &numThreads;
  tmpThr->next = nullptr;

  if (::posix_memalign(&tmpThr->stack, sysconf(_SC_PAGESIZE), stackSize) != 0) {
    if (tmpThr->name != nullptr)
      delete tmpThr->name;

    ::pthread_attr_destroy(&tmpThr->attr);
    delete tmpThr;
    return -1;
  }
  if (::pthread_attr_setstack(&tmpThr->attr, tmpThr->stack, stackSize) != 0) {
    if (tmpThr->name != nullptr)
      delete tmpThr->name;

    ::pthread_attr_destroy(&tmpThr->attr);
    ::free(tmpThr->stack);
    delete tmpThr;
    return -1;
  }

  tmpThr->status = buffioThreadStatus::configOk;
  tmpThr->stackSize = stackSize;
  tmpThr->func = func;

  if (::pthread_create(&tmpThr->id, &tmpThr->attr, buffioFunc, tmpThr) != 0) {
    if (tmpThr->name != nullptr)
      delete tmpThr->name;

    ::free(tmpThr->stack);
    ::pthread_attr_destroy(&tmpThr->attr);
    delete tmpThr;
    return -1;
  }
  /*
  if (name != nullptr) {
    size_t len = std::strlen(name);
    if (len < 14) {
      ::pthread_setname_np(tmpThr->id,"thread");
    }
  }
*/
  ::pthread_mutex_lock(&buffioMutex);
  if (threads == nullptr) {
    threads = tmpThr;
  } else {
    tmpThr->next = threads;
    threads = tmpThr;
  }
  ::pthread_mutex_unlock(&buffioMutex);

  return 0;
};

void *thread::buffioFunc(void *data) {
  struct threadinternal *tmpThr = reinterpret_cast<threadinternal *>(data);
  tmpThr->status.store(buffioThreadStatus::running, std::memory_order_release);

  tmpThr->numThreads->fetch_add(1, std::memory_order_acq_rel);

  int mutexEnabled = tmpThr->func(tmpThr->resource);

  tmpThr->status.store(buffioThreadStatus::done, std::memory_order_release);
  tmpThr->numThreads->fetch_add(-1, std::memory_order_acq_rel);

  ::pthread_exit(nullptr);
  return nullptr;
};
}; // namespace buffio
