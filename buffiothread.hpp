#ifndef __BUFFIO_THREAD_HPP__
#define __BUFFIO_THREAD_HPP__

#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>


class buffiothread {

public:
  buffiothread()
      : stack(nullptr), stacktop(nullptr), callfunc(nullptr), dataptr(nullptr),
        stacksize(0), threadstatus(BUFFIO_THREAD_NOT),threadname(nullptr) {}
    

  ~buffiothread() { reset(); };

  void reset() {
    switch (threadstatus) {
    case BUFFIO_THREAD_NOT:
      return;
      break;
    case BUFFIO_THREAD_RUNNING:
      killthread();
    case BUFFIO_THREAD_DONE:
    case BUFFIO_THREAD_ERROR:
      munmap(stack, stacksize);
    case BUFFIO_THREAD_ERROR_MAP:
      stacksize = 0;
      stack = stacktop = nullptr;
      threadname = nullptr; 
    };
    threadstatus = BUFFIO_THREAD_NOT;
  }

  void killthread() {
    if (pid < 0)
      return;
    switch (threadstatus) {
    case BUFFIO_THREAD_RUNNING:
      kill(pid, SIGKILL);
      break;
    }
    return;
  };

  void wait() {
    if (pid < 0)
      return;
    waitpid(pid, NULL, 0);
    return;
  }

  buffiothread &operator=(int (*func)(void *data)) {
    callfunc = func;
    return *this;
  }

  buffiothread &operator[](size_t stack) {
    if (stacksize == 0)
      stacksize = stack;
    return *this;
  };

  buffiothread &operator[](const char *name){
  if(threadname == nullptr) threadname = (char *)name; 
    return *this;
  };

  buffiothread &operator()(void *data) {
    dataptr = data;
    return *this;
  };
  buffiothread &run(){
    if(threadstatus != BUFFIO_THREAD_RUNNING){
       call();
    }
    return *this;
  };
  bool running(){ return (threadstatus == BUFFIO_THREAD_RUNNING);}
  bool done(){ return (threadstatus == BUFFIO_THREAD_DONE);}

  static int setname(const char *name) {
    return prctl(PR_SET_NAME, name, 0, 0, 0);
  };

  static constexpr size_t S1MB = 1024 * 1024;
  static constexpr size_t S4MB = 4 * (1024 * 1024);
  static constexpr size_t S9MB = 9 * (1024 * 1024);
  static constexpr size_t S10MB = 10 * (1024 * 1024);
  static constexpr size_t SD = buffiothread::S9MB;
  static constexpr size_t S1KB = 1024;

  
private:
  static int buffiofunc(void *data) {
    buffiothread *instance = (buffiothread *)data;
    if(instance->threadname) buffiothread::setname(instance->threadname);
    instance->callfunc(instance->dataptr);
    return 0;
  };

  void call() {
    if (stacksize < 0 || callfunc == nullptr)
      return;
    stack = (char *)mmap(NULL, stacksize, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    stacktop = stack + stacksize;

    if (stack == (void *)-1) {
      threadstatus = BUFFIO_THREAD_ERROR_MAP;
      BUFFIO_ERROR("Failed to allocate thread stack, aborting thread creation, reason -> "
                   ,strerror(errno));

      reset();
      return;
    }

    pid = clone(buffiofunc, stacktop,
                CLONE_FILES | CLONE_FS | CLONE_IO| CLONE_VM | SIGCHLD, this);

    if (pid < 0) {
      threadstatus = BUFFIO_THREAD_ERROR;
      BUFFIO_ERROR("Failed to create thread , reason -> "
                  ,strerror(errno),
                  threadname == nullptr ? "error":"thread name ->",threadname);
      
      reset();
      return;
    }
    threadstatus = BUFFIO_THREAD_RUNNING;
    return;
  };

  char *stack;
  char *threadname;
  char *stacktop;
  void *dataptr;
  int threadstatus;
  int numthread;
  pid_t pid;
  size_t stacksize;
  
  int (*callfunc)(void *);
};


#endif 
