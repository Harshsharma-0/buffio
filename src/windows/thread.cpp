#include "buffio/thread.hpp"

int buffio::semaphore::create(size_t initialValue) {

  LONG lmaxCount = static_cast<LONG>(initialValue);
  HANDLE semHandle = CreateSemaphoreA(NULL, lmaxCount, lmaxCount, NULL);

  lsem = semHandle;
  if (semHandle != INVALID_HANDLE_VALUE)
    return 0;

  /* HERE ONLY IF THERE IS ERROR CREATING SEMAPHORE */

  return B_ESEMCREATE;
};

int buffio::semaphore::post() {

  LONG val = 0;
  if (!ReleaseSemaphore(lsem, 1, &val))
    return buffio::error_from_os(GetLastError());

  return 0;
};

int buffio::semaphore::wait() {
  LONG val = 0;
  if (!WaitForSingleObject(lsem, -1))
    return buffio::error_from_os(GetLastError());
  return 0;
};

void buffio::semaphore::destroy() { CloseHandle(lsem); };

int buffio::thread::run(buffio::threadFuncSig start, void *args) {

  size_t stackSize = 5 * 1024 * 1024;
  this->routine = start;
  this->args = args;

  auto threadMainRoutine = [](LPVOID args) -> DWORD WINAPI {
    buffio::thread *instance = static_cast<buffio::thread *>(args);
    instance->routine(instance->args);
    return 0;
  };

  LPVOID pArgs = static_cast<LPVOID>(this);
  HANDLE threadHandle = CreateThread(NULL, stackSize, threadMainRoutine, pArgs,
                                     STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);

  if (threadHandle == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    if (ERROR_NOT_ENOUGH_MEMORY == error) {
      return B_ETHREADNOMEM;
    };
    if (ERROR_INVALID_PARAMETER == error) {
      return B_ETHREADARGS;
    }
    return B_ETHREAD;
  };

  this->handle = threadHandle;

  return 0;
};

int buffio::thread::join(uint32_t timeout) {

  DWORD res = WaitForSingleObject(handle, static_cast<DWORD>(timeout));
  DWORD exitCode = 0;

  switch (res) {
  case WAIT_OBJECT_0:
    return 0;
    break;
  case WAIT_TIMEOUT:
    return B_ETIMEDOUT;
    break;
  case WAIT_FAILED:
    return buffio::error_from_os(GetLastError());
    break;
  };
  return B_EUNKNOWN;
};
