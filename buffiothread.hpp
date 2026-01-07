#ifndef __BUFFIO_THREAD_HPP__
#define __BUFFIO_THREAD_HPP__

#include <atomic>
#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>


// The struct buffiothreadinfo lifecycle after running the thread is upon user if they want to keep it or not
// but when running thread struct buffiothreadinfo must be valid
struct buffiothreadinfo{
 size_t stacksize;
 void *dataptr;
 char *threadname;
 pid_t pid;
 int (*callfunc)(void*);
 int idx;
};

class buffiothread {

#define threadstackok 1
#define threadfuncok 1 << 1
#define threaddataptrok 1 << 2
#define threadnameok 1 << 3

#define maskok (threadstackok | threadfuncok | threaddataptrok | threadnameok)

struct threadinfo{
    char *stack;
    char *stacktop;
    struct buffiothreadinfo threadint;
    int stalemask;
    std::atomic<int> threadstatus;
};

public:
  buffiothread()
      : threads(nullptr),numthread(0){ }

    
  ~buffiothread() { 
    killthread(-1);
    delete[] threads;
    threads = nullptr;
  };

   
  void killthread(int id) {
    if (threads == nullptr)
       return;

    if(id < 0){
    for(int i = 0; i < numthread ; i++) derefer(&threads[i]);
      return;
    }
    if(id >= 0 && id < numthread)
        derefer(&threads[id]);

     return;
  };

  int runthreads(int n,struct buffiothreadinfo *__threads){
   if(n <= 0 || !__threads) return -1;
   if(threads != nullptr) return -2;

   threads = new struct threadinfo[n];
   numthread = n;

   for(int i = 0; i < n ; i++){
     threads[i].threadstatus = BUFFIO_THREAD_NOT;
     threads[i].stalemask = 0;
     if(__threads[i].callfunc){
         threads[i].threadint.callfunc = __threads[i].callfunc; 
         threads[i].stalemask |= threadfuncok;

      }
      if(__threads[i].threadname){
         threads[i].threadint.threadname = __threads[i].threadname;
         threads[i].stalemask |= threadnameok;

      }
      if(__threads[i].stacksize > buffiothread::S1KB){
        threads[i].threadint.stacksize = __threads[i].stacksize;
        threads[i].stalemask |= threadstackok;
      }
      threads[i].threadint.dataptr = __threads[i].dataptr;
      threads[i].stalemask |= threaddataptrok;
      __threads[i].idx = -1;
      if(call(&threads[i]) > 0){
        __threads[i].pid = threads[i].threadint.pid;
        __threads[i].idx = i; 
      }
   }
    return 0;
  }
  void wait(pid_t id){waitpid(id,0,0);}
  void waitidx(int idx){
    if(idx >= 0 && idx < numthread){
       if(threads[idx].threadstatus.load(std::memory_order_acquire) == BUFFIO_THREAD_RUNNING)
           waitpid(threads[idx].threadint.pid,0,0);
    }
  }
  void waitall(){
    for(int i = 0; i < numthread; i++){
      switch(threads[i].threadstatus.load(std::memory_order_acquire)){
       case BUFFIO_THREAD_RUNNING:
       waitpid(threads[i].threadint.pid,0,0);
       break;
      }
    }
  }


  static int setname(const char *name) {
    return prctl(PR_SET_NAME, name, 0, 0, 0);
  };
  buffiothread(const buffiothread&) = delete;
  buffiothread &operator= () = delete;

  static constexpr size_t S1MB = 1024 * 1024;
  static constexpr size_t S4MB = 4 * (1024 * 1024);
  static constexpr size_t S9MB = 9 * (1024 * 1024);
  static constexpr size_t S10MB = 10 * (1024 * 1024);
  static constexpr size_t SD = buffiothread::S9MB;
  static constexpr size_t S1KB = 1024;

  
private:
  // derefer is used both to kill and unmap a thread, and the cases are handled in that manner, if a thread is marked killed it is not
  // use and not handled by any case it the thread is done only unmapping is done and it the thread is running it is killed and the unmapped
  void derefer(struct threadinfo *info){
    if(info->stalemask == maskok){ 
      switch(info->threadstatus.load(std::memory_order_acquire)){
        case BUFFIO_THREAD_RUNNING:
            kill(info->threadint.pid,SIGKILL);
            waitpid(info->threadint.pid,0,0);
            [[fallthrough]];
            case BUFFIO_THREAD_DONE:
            munmap(info->stack,info->threadint.stacksize);
            info->threadstatus = BUFFIO_THREAD_KILLED;
            info->stalemask = 0;
        break;
      }
    }
  };

  static int buffiofunc(void *data) {
     struct threadinfo *instance = (struct threadinfo *)data;
     if(instance->threadint.threadname) buffiothread::setname(instance->threadint.threadname);
     // the return value will be propageted to the func in future update
     // TODO: propagete the return value
     instance->threadint.callfunc(instance->threadint.dataptr);
     instance->threadstatus.store(BUFFIO_THREAD_DONE,std::memory_order_release);
     return 0;
  };

  int call(struct threadinfo *which){
   

    if(which->stalemask & maskok) == maskok){
    char *stack = nullptr, *stacktop = nullptr;
    size_t stacksize = which->threadint.stacksize;
    pid_t pid = 0;

    stack = (char *)mmap(NULL,stacksize, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);

    if (stack == (void *)-1) {
      which->threadstatus = BUFFIO_THREAD_ERROR_MAP;
      BUFFIO_ERROR("Failed to allocate thread stack, aborting thread creation, reason -> "
                   ,strerror(errno));

      return -1;
    }
       stacktop = stack + stacksize;
       pid = clone(buffiofunc, stacktop,
                CLONE_FILES | CLONE_FS | CLONE_IO| CLONE_VM | CLONE_THREAD | SIGCHLD, which);

    if (pid < 0) {
      which->threadstatus = BUFFIO_THREAD_ERROR;
      BUFFIO_ERROR("Failed to create thread , reason -> "
                  ,strerror(errno),
                  which->threadint.threadname == nullptr ? "error":"thread name ->",which->threadint.threadname);
      

      return -1;
    }
       int tmpstatus = BUFFIO_THREAD_NOT;
       which->stacktop = stacktop;
       which->stack = stack;
       which->threadstatus.compare_exchange_weak(tmpstatus,BUFFIO_THREAD_RUNNING,std::memory_order_acq_rel);
       which->threadint.pid = pid;
     };

    return 1;
  };

  struct threadinfo *threads;
  int numthread;
};


#endif 
