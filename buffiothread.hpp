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

#define threadstackok 1
#define threadfuncok 1 << 1
#define threaddataptrok 1 << 2
#define threadnameok 1 << 3

#define maskok threadstackok | threadfuncok | threaddataptrok | threadnameok

struct threadinfo{
    char *stack;
    char *threadname;
    char *stacktop;
    void *dataptr;
    size_t stacksize;
    pid_t pid;
    int (*callfunc)(void *);
    int stalemask;
    int threadstatus;

};

public:
  buffiothread(int nthread)
      : threads(nullptr),numthread(nthread),
        filledslot(0),currentinctx(0){
        setN(nthread);
   }

  buffiothread()
      : threads(nullptr), 
        numthread(0),
        filledslot(0),
        currentinctx(0){}
    

  ~buffiothread() { reset(); };

  void reset() {
    switch (threadstatus) {
    case BUFFIO_THREAD_NOT:
      return;
      break;
    case BUFFIO_THREAD_RUNNING:
    case BUFFIO_THREAD_DONE:
      killthread(0);
    case BUFFIO_THREAD_ERROR:
    case BUFFIO_THREAD_ERROR_MAP:
      if(threads != nullptr) delete threads;
    };
    threadstatus = BUFFIO_THREAD_NOT;
  }
  
  void killthread(int id) {
    if (threads == nullptr)
      return;

  
    if(id <= filledslot && id > 0){
     struct threadinfo *info = (threads + (id - 1));
     if(info->stalemask == maskok && info->threadstatus == BUFFIO_THREAD_RUNNING){
               kill(info->pid,SIGKILL);
               info->threadstatus = BUFFIO_THREAD_KILLED;
               munmap(info->stack,info->stacksize);
               return; 
      }
    }

   if(id == 0){  
   for(int i = 0; i < filledslot ; i++){
    struct threadinfo *info = (threads + i);
    switch (info->threadstatus) {
    case BUFFIO_THREAD_RUNNING:
      kill(info->pid, SIGKILL);
      info->threadstatus = BUFFIO_THREAD_KILLED;
    case BUFFIO_THREAD_DONE:
      munmap(info->stack,info->stacksize);
      break;
     }
    }
    filledslot = 0;
  }
    return;
  };

  int setN(int nthread){
    if(nthread > 0 && threads == nullptr){
      threads = new struct threadinfo[nthread];
      numthread = nthread;
      if(threads == nullptr) return -1;
      struct threadinfo ctx = {0};
    
      for(int i = 0; i < nthread ; i++)
         threads[i].threadstatus = BUFFIO_THREAD_NOT;
         
      return 0;
    }
    return -2;
  }

  void wait(int id){
    if(id == 0){
      for(int i = 0; i < filledslot;i++){
        switch(threads[i].threadstatus){
          case BUFFIO_THREAD_RUNNING:
            waitpid(threads[i].pid,0,0);
          break;
        }
      }
         
    }
    return;
  }
 /* Example on how to use the thread library
  * #include "buffiothread.hpp"
  *  static int func(void data){
  *  return 0;
  * }
  * int main(){
  *  buffiothread thread(1);
  *  //  1 = the id of the thread in space allocated
  *  // before starting a thread if must be given and end the thread conf with
  *  // '=' to assign the function to the thread
  *  thread(1)("test task")[stacksize](nullptr) = func;
  * }
  * 
  *
  */

  buffiothread &operator()(int id){
    if(id > 0 && id <= numthread){ 
      currentinctx = (id - 1);
      return *this;
    }
    currentinctx = -1;
    return *this;
  }

  buffiothread &operator=(int (*func)(void *data)){
    if(currentinctx == -1) return *this;
    threads[currentinctx].callfunc = func;
    threads[currentinctx].stalemask |= threadfuncok;
    currentinctx = -1;
    filledslot += 1;
    return *this;
  }

  buffiothread &operator[](size_t stack){
    if(currentinctx == -1) return *this;

    if (threads[currentinctx].stacksize == 0 && stack > 1024){
     threads[currentinctx].stacksize = stack;
     threads[currentinctx].stalemask |= threadstackok;
    }

    return *this;
  };

  buffiothread &operator[](const char *name){
  if(currentinctx == -1) return *this;
  if(threads[currentinctx].threadname == nullptr)
      threads[currentinctx].threadname = (char *)name;

   threads[currentinctx].stalemask |= threadnameok;

    return *this;
  };

  buffiothread &operator()(void *data) {
    if(currentinctx == -1) return *this;

    threads[currentinctx].dataptr = data;
    threads[currentinctx].stalemask |= threaddataptrok;

    return *this;
  };

   int run(int id){
    // special case to run all thread
    if(id == 0){ 
      for(int i = 0; i < filledslot;){
        i += 1;
        call((threads+(i -1)));
      }
      return 0;
    }
    if(id < filledslot){
      call(threads + (filledslot - 1));
      return 0;
    }
    return -1;
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
     struct threadinfo *instance = (struct threadinfo *)data;
     if(instance->threadname) buffiothread::setname(instance->threadname);
     instance->callfunc(instance->dataptr);
     instance->threadstatus = BUFFIO_THREAD_DONE;
     return 0;
  };

  void call(struct threadinfo *which){
    if (filledslot <= 0 || which == nullptr)
      return;

    if(which->stalemask == (maskok) && which->threadstatus != BUFFIO_THREAD_RUNNING){
    char *stack = nullptr, *stacktop = nullptr;
    size_t stacksize = which->stacksize;
    pid_t pid = 0;

    stack = (char *)mmap(NULL,stacksize, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    stacktop = stack + stacksize;

    if (stack == (void *)-1) {
      which->threadstatus = BUFFIO_THREAD_ERROR_MAP;
      BUFFIO_ERROR("Failed to allocate thread stack, aborting thread creation, reason -> "
                   ,strerror(errno));

      reset();
      return;
    }
    which->stacktop = stacktop;
    which->stack = stack;
    which->pid = pid;

    which->threadstatus = BUFFIO_THREAD_RUNNING;
    pid = clone(buffiofunc, stacktop,
                CLONE_FILES | CLONE_FS | CLONE_IO| CLONE_VM | SIGCHLD, which);

    if (pid < 0) {
      which->threadstatus = BUFFIO_THREAD_ERROR;
      BUFFIO_ERROR("Failed to create thread , reason -> "
                  ,strerror(errno),
                  which->threadname == nullptr ? "error":"thread name ->",which->threadname);
      
      reset();
      return;
    }
     };
    return;
  };

  struct threadinfo *threads;
  int threadstatus;
  int numthread;
  int filledslot;
  int currentinctx;
};


#endif 
