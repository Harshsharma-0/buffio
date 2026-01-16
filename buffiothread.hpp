#ifndef __BUFFIO_THREAD_HPP__
#define __BUFFIO_THREAD_HPP__

/*
* Error codes range reserved for buffiothread
*  [6000 - 7500]
*  6000 <= errorcode <= 7500
*/

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

enum class buffio_threadinfo_state: uint32_t{
 none     = 0,
 stack_ok = 1u,
 func_ok  = 1u << 1,
 data_ok  = 1u << 2,
 name_ok  = 1u << 3,
};

constexpr buffio_threadinfo_state operator|(buffio_threadinfo_state a, 
                                               buffio_threadinfo_state b){
  return static_cast<buffio_threadinfo_state>
                  (static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
};

constexpr buffio_threadinfo_state& operator|=(buffio_threadinfo_state& lhs ,
                                                    buffio_threadinfo_state rhs){
     lhs = lhs | rhs;
     return lhs;
}

constexpr buffio_threadinfo_state operator&(buffio_threadinfo_state  a,
                                             buffio_threadinfo_state  b)
{
    return static_cast<buffio_threadinfo_state>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
};

constexpr buffio_threadinfo_state thread_state_ok = buffio_threadinfo_state::stack_ok |
                                                    buffio_threadinfo_state::func_ok  |
                                                    buffio_threadinfo_state::data_ok  |
                                                    buffio_threadinfo_state::name_ok;
class buffiothread {


struct threadinfo{
    char *stack;
    char *stacktop;
    struct buffiothreadinfo threadint;
    buffio_threadinfo_state thread_mask;
    std::atomic<buffio_thread_status> threadstatus;
};

public:
  buffiothread(): threads(nullptr),numthread(0){ }

    
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
   if(threads == nullptr) return -3;
   numthread = n;

   for(int i = 0; i < n ; i++){
     threads[i].threadstatus = buffio_thread_status::inactive;
     threads[i].thread_mask = buffio_threadinfo_state::none;
     if(__threads[i].callfunc){
         threads[i].threadint.callfunc = __threads[i].callfunc; 
         threads[i].thread_mask |= buffio_threadinfo_state::func_ok;

      }
      if(__threads[i].threadname){
         threads[i].threadint.threadname = __threads[i].threadname;

      }
         threads[i].thread_mask |= buffio_threadinfo_state::name_ok;

      if(__threads[i].stacksize > buffiothread::S1KB){
        threads[i].threadint.stacksize = __threads[i].stacksize;
        threads[i].thread_mask |= buffio_threadinfo_state::stack_ok;

      }
      threads[i].threadint.dataptr = __threads[i].dataptr;
      threads[i].thread_mask |= buffio_threadinfo_state::data_ok;
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
       if(threads[idx].threadstatus.load(std::memory_order_acquire)
                 == buffio_thread_status::running)
           waitpid(threads[idx].threadint.pid,0,0);
    }
  }
  void waitall(){
    for(int i = 0; i < numthread; i++){
      switch(threads[i].threadstatus.load(std::memory_order_acquire)){
       case buffio_thread_status::running:
       waitpid(threads[i].threadint.pid,0,0);
       break;
      }
    }
  }


  static int setname(const char *name) {
    return prctl(PR_SET_NAME, name, 0, 0, 0);
  };
  buffiothread(const buffiothread&) = delete;

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
    if(info->thread_mask == thread_state_ok){ 
      switch(info->threadstatus.load(std::memory_order_acquire)){
        case buffio_thread_status::running:
            kill(info->threadint.pid,SIGKILL);
            waitpid(info->threadint.pid,0,0);
            [[fallthrough]];
            case buffio_thread_status::done:
            munmap(info->stack,info->threadint.stacksize);
            info->threadstatus = buffio_thread_status::killed;
            info->thread_mask = buffio_threadinfo_state::none;
        break;
      }
    }
  };

  static int buffiofunc(void *data) {
     struct threadinfo *instance = (struct threadinfo *)data;
     if(instance->threadint.threadname) 
         buffiothread::setname(instance->threadint.threadname);
     // the return value will be propageted to the func in future update
     // TODO: propagete the return value
     instance->threadint.callfunc(instance->threadint.dataptr);
     instance->threadstatus.store(buffio_thread_status::done,std::memory_order_release);
     return 0;
  };

  int call(struct threadinfo *which){
   

    if((which->thread_mask & thread_state_ok) == thread_state_ok){
    char *stack = nullptr, *stacktop = nullptr;
    size_t stacksize = which->threadint.stacksize;
    pid_t pid = 0;

    stack = (char *)mmap(NULL,stacksize, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);

    if (stack == (void *)-1) {
      which->threadstatus = buffio_thread_status::error_map;
      BUFFIO_ERROR("Failed to allocate thread stack, aborting thread creation, reason -> "
                   ,strerror(errno));

       return -1;
    }
       stacktop = stack + stacksize;
       pid = clone(buffiofunc, stacktop,
                CLONE_FILES | CLONE_FS | CLONE_IO| CLONE_VM | SIGCHLD, which);
    if (pid < 0) {
      which->threadstatus = buffio_thread_status::error;
      BUFFIO_ERROR("Failed to create thread , reason -> "
                  ,strerror(errno),
                   which->threadint.threadname == nullptr ? ", error":", thread name ->",
                   which->threadint.threadname); 

      return -1;
    }

       buffio_thread_status tmpstatus = buffio_thread_status::inactive;
       which->stacktop = stacktop;
       which->stack = stack;
       which->threadstatus.compare_exchange_weak(tmpstatus,buffio_thread_status::running
                                                     ,std::memory_order_acq_rel);
       which->threadint.pid = pid;
     };

    return 1;
  };

  struct threadinfo *threads;
  int numthread;
};


#endif 
