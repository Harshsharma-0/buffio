#include "buffio/thread.hpp"

#define BUFFIO_THREAD_RETURN BUFFIO_OS_INSERT(void *, void *, DWORD WINAPI)
#define BUFFIO_THREAD_ARGS BUFFIO_OS_INSERT(void *, void *, LPVOID) args

int buffio::thread::run(buffio::threadFuncSig start, void *args) {

  size_t stackSize = 5 * 1024 * 1024;
  this->routine = start;
  this->args = args;

  auto threadMainRoutine = [](BUFFIO_THREAD_ARGS) -> BUFFIO_THREAD_RETURN {
    buffio::thread *instance = static_cast<buffio::thread *>(args);
    instance->routine(instance->args);
    return BUFFIO_OS_INSERT(nullptr, nullptr, 0);
  };

#if defined(BUFFIO_OS_LINUX) || defined(BUFFIO_OS_BSD)

  pthread_attr_t attribute;
  pthread_attr_init(&attribute);
  pthread_attr_setstacksize(&attribute, stackSize);
  void *pArgs = static_cast<void *>(this);
  if (pthread_create(&this->handle, &attribute, threadMainRoutine, pArgs) !=
      0) {
    /* return BENOMEM from here */
    if (EAGAIN == errno) {
      return -1;
    };
    /* return BEUNKNOWM from here */
    return -1;
  };

  pthread_attr_destroy(&attribute);

#elif defined(BUFFIO_OS_WINDOW)
  LPVOID pArgs = static_cast<LPVOID>(this);
  HANDLE threadHandle = createThread(NULL, stackSize, buffioWorkerFunc, pArgs,
                                     STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);

  if (threadHandle != NULL) {
    /* return BENOMEM from here */
    if (ERROR_NOT_ENOUGH_MEMORY == GetLastError()) {
      return -1;
    };
    /* return BEUNKNOWM from here */
    return -1;
  };

  this->handle = threadHandle;

#else

#error unsupported platform

#endif

  /* HERE ONLY IF SUCCESS */

  return 0;
};

int buffio::thread::join(){

  BUFFIO_OS_INSERT(
      pthread_join(handle,NULL);
      return 0,
      return -1,
      return -1
      );
};

#undef BUFFIO_THREAD_RETURN
#undef BUFFIO_THREAD_ARGS
