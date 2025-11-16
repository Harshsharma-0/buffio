#ifndef BUFF_IO
#define BUFF_IO

#include "./buffiolog.hpp"

#include <coroutine>
#include <cstdint>
#include <exception>
#include <fcntl.h>
#include <sys/socket.h>
#include <unordered_map>

constexpr int BUFFIO_FAMILY_LOCAL = AF_UNIX;
constexpr int BUFFIO_FAMILY_IPV4 = AF_INET;
constexpr int BUFFIO_FAMILY_IPV6 = AF_INET6;
constexpr int BUFFIO_FAMILY_CAN = AF_CAN;
constexpr int BUFFIO_FAMILY_NETLINK = AF_NETLINK;
constexpr int BUFFIO_FAMILY_LLC = AF_LLC;
constexpr int BUFFIO_FAMILY_BLUETOOTH = AF_BLUETOOTH;

constexpr int BUFFIO_SOCK_TCP = SOCK_STREAM;
constexpr int BUFFIO_SOCK_UDP = SOCK_DGRAM;
constexpr int BUFFIO_SOCK_RAW = SOCK_RAW;
constexpr int BUFFIO_SOCK_ASYNC = SOCK_NONBLOCK;


enum BUFFIO_ROUTINE_STATUS {
  BUFFIO_ROUTINE_STATUS_WAITING = 21,
  BUFFIO_ROUTINE_STATUS_EXECUTING,
  BUFFIO_ROUTINE_STATUS_YIELD,
  BUFFIO_ROUTINE_STATUS_ERROR,
  BUFFIO_ROUTINE_STATUS_PAUSED,
  BUFFIO_ROUTINE_STATUS_DONE,
  BUFFIO_ROUTINE_STATUS_EHANDLER,
  BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION,
  BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION_DONE,
};

enum BUFFIO_TASK_STATUS {
  BUFFIO_TASK_SWAPPED = 31,
  BUFFIO_TASK_WAITER_EXCEPTION_WAITING,
  BUFFIO_TASK_WAITER_EXCEPTION_DONE,
  BUFFIO_TASK_WAITER_NONE,
};

enum BUFFIO_QUEUE_STATUS {
  BUFFIO_QUEUE_STATUS_ERROR = 40,
  BUFFIO_QUEUE_STATUS_SUCCESS = 41,
  BUFFIO_QUEUE_STATUS_YIELD,
  BUFFIO_QUEUE_STATUS_EMPTY,
  BUFFIO_QUEUE_STATUS_SHUTDOWN,
  BUFFIO_QUEUE_STATUS_CONTINUE,
};

enum BUFFIO_EVENTLOOP_TYPE {
  BUFFIO_EVENTLOOP_SYNC = 50, // use this to block main thread
  BUFFIO_EVENTLOOP_ASYNC,     // use this to launch a thread;
  BUFFIO_EVENTLOOP_SEPERATE,  // use this to create a seperate process from main
  BUFFIO_EVENTLOOP_DOWN,      // indicates eventloop is not running

};

enum BUFFIO_ACCEPT_STATUS {
  BUFFIO_ACCEPT_STATUS_ERROR = 60,
  BUFFIO_ACCEPT_STATUS_SUCCESS = 61,
  BUFFIO_ACCEPT_STATUS_NA,
  BUFFFIO_ACCEPT_STATUS_NO_HANDLER,
};

#if defined(BUFFIO_IMPLEMENTATION)

struct buffioinfo {
  const char *address;
  int portnumber;
  int listenbacklog;
  int socktype;
  int sockfamily;
};

struct buffiobuffer {
  char *data;
  size_t filled;
  size_t size;
};

struct clientinfo {
  const char *address;
  int clientfd;
  int portnumber;
  buffiobuffer readbuffer;
  buffiobuffer writebuffer;
};

struct buffiopromise;
using buffiohandleroutine = std::coroutine_handle<buffiopromise>;
#define buffiowait co_await
#define buffioyeild co_yield
#define buffioreturn co_return
#define buffiopush co_await

struct buffioroutine : buffiohandleroutine {
  using promise_type = ::buffiopromise;
};

struct acceptreturn {
  int errorcode;
  buffioroutine handle;
};

struct buffioawaiter {
  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> h) noexcept {};
  buffioroutine await_resume() noexcept { return self; };
  buffioroutine self;
};

struct buffiopushtaskinfo {
  buffioroutine task;
};

struct buffiopromisestatus {
  enum BUFFIO_ROUTINE_STATUS status;
  int returncode = 0;
  std::exception_ptr routineexception;
};

struct buffiopromise {
  buffioroutine waitingfor;
  buffioroutine pushhandle;
  buffioroutine self;
  buffiopromisestatus childstatus;
  buffiopromisestatus selfstatus;

  std::exception_ptr routineexception;

  buffioroutine get_return_object() {
    self = {buffioroutine::from_promise(*this)};
    selfstatus.status = BUFFIO_ROUTINE_STATUS_EXECUTING;
    return self;
  };

  std::suspend_always initial_suspend() noexcept { return {}; };
  std::suspend_always final_suspend() noexcept { return {}; };
  std::suspend_always yield_value(int value) {
    selfstatus.status = BUFFIO_ROUTINE_STATUS_YIELD;
    return {};
  };

  buffioawaiter await_transform(buffioroutine waitfor) {
    waitingfor = waitfor;
    selfstatus.status = BUFFIO_ROUTINE_STATUS_WAITING;
    return {.self = self};
  };

  buffioawaiter await_transform(buffiopushtaskinfo info) {
    pushhandle = info.task;
    selfstatus.status = BUFFIO_ROUTINE_STATUS_PAUSED;
    return {.self = info.task};
  };

  void unhandled_exception() {
    selfstatus.status = BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION;
    selfstatus.returncode = -1;
    selfstatus.routineexception = std::current_exception();
  };

  void return_value(int state) {
    selfstatus.returncode = state;
    selfstatus.status =
        state < 0 ? BUFFIO_ROUTINE_STATUS_ERROR : BUFFIO_ROUTINE_STATUS_DONE;
    return;
  };
  bool checkstatus() {
    return selfstatus.status == BUFFIO_ROUTINE_STATUS_ERROR ||
                   selfstatus.returncode < 0
               ? true
               : false;
  };
};

class buffiocatch {

public:
  buffiocatch(buffioroutine self) : evalue(self) {
    status = self.promise().childstatus;
  };
  void exceptionthrower() {
    switch (status.status) {
    case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
      std::rethrow_exception(status.routineexception);
      break;
    case BUFFIO_ROUTINE_STATUS_ERROR:
      throw std::runtime_error(
          "error in execution of routine return code less than 0");
      break;
    case BUFFIO_ROUTINE_STATUS_DONE:
      return;
      break;
    }
  }
  buffiocatch &throwerror() {
    exceptionthrower();
    return *this;
  };

  void operator=(void (*handler)(const std::exception &e, int successcode)) {
    if (evalue.promise().checkstatus()) {
      try {
        exceptionthrower();
      } catch (const std::exception &e) {
        handler(e, status.returncode);
      }
    }
  };

private:
  buffioroutine evalue;
  buffiopromisestatus status;
};

#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <atomic>
#include "./buffiosock.hpp"

struct buffiotaskinfo {
  buffioroutine handle;
  struct buffiotaskinfo *next;
  struct buffiotaskinfo *prev;
};

template <typename T, typename Y> class buffioqueue {
public:
  buffioqueue()
      : taskqueue(nullptr), taskqueuetail(nullptr), freehead(nullptr),
        freetail(nullptr), tasknext(nullptr), waitingtaskcount(0),
        waitingtaskhead(nullptr), waitingtasktail(nullptr),
        executingtaskcount(0), totaltaskcount(0), freetaskcount(0),
        activetaskcount(0)

  {
    queueerror = BUFFIO_QUEUE_STATUS_EMPTY;
  };

  ~buffioqueue() {
    if (executingtaskcount != (activetaskcount + waitingtaskcount))
      BUFFIO_INFO("QUEUE : memory leaked. or entry lost: ", executingtaskcount,
                  " : active - ", activetaskcount, ": waiting - ",
                  waitingtaskcount, " : freecount - ", freetaskcount);

    buffioclean(taskqueue);
    buffioclean(freehead);
  };

  void inctotaltask() {
    totaltaskcount++;
    activetaskcount++;
    executingtaskcount++;
  }

  T *pushroutine(Y routine) {
    T *tmproutine = popfreequeue();
    tmproutine->handle = routine;
    pushtaskptr(tmproutine, &taskqueue, &taskqueuetail);
    inctotaltask();
    return tmproutine;
  };

  void reschedule(T *task) { pushtaskptr(task, &taskqueue, &taskqueuetail); };

  T *getnextwork() {
    T *t = taskqueue;
    erasetaskptr(t, &taskqueue, &taskqueuetail);
    return t;
  }

  void settaskwaiter(T *task, Y routine) {
    if (task == nullptr)
      return;
    waitingmap[pushroutine(routine)] = task;

    waitingtaskcount++;
    activetaskcount--;
    executingtaskcount--;
    return;
  };

  T *poptaskwaiter(T *task) {

    if (task == nullptr)
      return nullptr;
    auto handle = waitingmap.find(task);
    if (handle == waitingmap.end())
      return nullptr;
    reschedule(handle->second);

    activetaskcount++;
    executingtaskcount++;
    waitingtaskcount--;
    return handle->second;
  };

  void poptask(T *task) {
    pushtaskptr(task, &freehead, &freetail);
    activetaskcount--;
    executingtaskcount--;
  };

  bool empty() { return (executingtaskcount == 0); }
  size_t taskn() { return totaltaskcount; };
  int getqueuerrror() { return queueerror; }

private:
  void buffioclean(T *head) {
    if (head == nullptr)
      return;
    if(head = head->next){ delete head; return;}

    T *tmp = head;
    while (tmp != nullptr) {
      tmp = head->next;
      delete head;
      head = tmp;
    }
  };

  // checkfor nullptr task before entering this function;
  void pushtaskptr(T *task, T **head, T **tail) {
    // indicates a empty list; insertion in empty list:
    if (*head == nullptr && *tail == nullptr) {
      *head = task;
      *tail = task;
      task->next = task->prev = task;
      return;
    };

    
    // insertion in list of one element;
    if (*head == *tail) {
      (*head)->next = task;
      task->prev = *head;
      *tail = task;
      return;
    };
    

    // insertion in a list of element greater than 1;
 
    (*tail)->next = task;
    task->prev = *tail;
    *tail = task;

    return;
  };

  void erasetask(T *task) { erasetaskptr(task, &taskqueue, &taskqueuetail); }

  void erasetaskptr(T *task, T **head, T **tail) {
    if (task == nullptr || *head == nullptr)
      return;

    // only element
    if (*head == *tail) {
      *head = *tail = nullptr;
      task->next = task->prev = nullptr;
      return;
    }

  
    // removing head
    if (task == *head) {
      *head = task->next;
      (*head)->prev = nullptr;
      task->next = task->prev = nullptr;
      return;
    }

    // removing tail
    if (task == *tail) {
      *tail = task->prev;
      (*tail)->next = nullptr;
      task->next = task->prev = nullptr;
      return;
    }
    

    // removing from middle of the queue
    task->prev->next = task->next;
    task->next->prev = task->prev;
    task->next = task->prev = nullptr;
  }

  T *popfreequeue() {
    if (freehead == nullptr) {
      T *t = new T;
      t->next = nullptr;
      t->prev = nullptr;
      return t;
    };
    T *ret = freehead;
    erasetaskptr(ret, &freehead, &freetail);

    return ret;
  }

  T *tasknext;
  T *taskqueue, *taskqueuetail;
  T *waitingtaskhead, *waitingtasktail;
  T *freehead, *freetail;
  int queueerror;
  size_t executingtaskcount, totaltaskcount;
  size_t activetaskcount, waitingtaskcount;
  size_t freetaskcount;
  std::unordered_map<T *, T *> waitingmap;
};

#include <sys/epoll.h>
// socket broker used to listen for events in socket;
// internally uses epoll for all work;
constexpr int BUFFIO_POLL_READ = EPOLLIN;
constexpr int BUFFIO_POLL_WRITE = EPOLLOUT;
constexpr int BUFFIO_POLL_ETRIG = EPOLLET;

#define BUFFIO_EPOLL_MAX_THRESHOLD 100

enum BUFFIO_SOCKBROKER_STATE {
  BUFFIO_SOCKBROKER_ACTIVE = 71,
  BUFFIO_SOCKBROKER_INACTIVE,
  BUFFIO_SOCKBROKER_BUSY,
  BUFFIO_SOCKBROKER_ERROR,
  BUFFIO_SOCKBROKER_SUCCESS,
};

struct buffiosbrokerinfo {
  int fd;
  int event;
  buffiotaskinfo *task;
};



class buffiosockbroker {
public:
  buffiosockbroker() : sbrokerstate(BUFFIO_SOCKBROKER_INACTIVE), fdcount(0),
                       eventcount(0){
    memset(&events,'\0',sizeof(epoll_event) * BUFFIO_EPOLL_MAX_THRESHOLD);
    consumed = 0;
  };

  int start() {
    switch (sbrokerstate) {
    case BUFFIO_SOCKBROKER_INACTIVE:
      epollfd = epoll_create(0);
      if (epollfd < 0) {
        BUFFIO_ERROR("Failed to create a epoll instance of socker : reason -> ",
                     strerrorno(errno))
        return BUFFIO_SOCKBROKER_ERROR;
      };
      break;
    case BUFFIO_SOCKBROKER_ACTIVE:
    case BUFFIO_SOCKBROKER_BUSY:
      break;
    }
    return BUFFIO_SOCKBROKER_SUCCESS;
  };

  int push(buffiosbrokerinfo *broker) {
    struct epoll_event event;
    event.events = broker->event;
    event.data.fd = broker->fd;
    event.data.ptr = static_cast<void *>(broker->task);
    switch (sbrokerstate) {
    case BUFFIO_SOCKBROKER_ACTIVE:
        int ret = epoll_ctl(epollfd,EPOLL_CTL_ADD,broker->fd,&event);
        if(ret < 0){
          BUFFIO_ERROR("Failed to add file descriptor in epoll, reason : ",strerrno(errno));
          break;
        } 
        return BUFFIO_SOCKBROKER_SUCCESS;
      break;
    } 
    return BUFFIO_SOCKBROKER_ERROR;
  };
  
  static int epolllistener(buffiosockbroker *selfinstance){
    return 0; 
  };

  ~buffiosockbroker() {
    switch (sbrokerstate) {
    case BUFFIO_SOCKBROKER_INACTIVE:
    case BUFFIO_SOCKBROKER_ACTIVE:
    case BUFFIO_SOCKBROKER_BUSY:
      break;
    }
  };

private:
  int sbrokerstate;
  int epollfd;
  size_t fdcount;
  size_t eventcount;
  std::atomic <int>consumed;
  struct epoll_event events[BUFFIO_EPOLL_MAX_THRESHOLD];
};
/*
 */

void buffioescalatetaskerror(buffiotaskinfo *to, buffiotaskinfo *from) {
  if (to == nullptr || from == nullptr)
    return;
  to->handle.promise().childstatus = from->handle.promise().selfstatus;
};

class buffioinstance {

public:
  static int eventloop(void *data) {
    buffioinstance *instance = (buffioinstance *)data;
    buffioqueue<buffiotaskinfo, buffioroutine> *queue = &instance->iqueue;
    while (queue->empty() == false) {
      buffiotaskinfo *taskinfo = queue->getnextwork();
      if (taskinfo == nullptr)
        break;
      buffioroutine taskhandle = taskinfo->handle;
      buffiopromise *promise = &taskhandle.promise();

      // executing task only when status is executing to avoid error;
      if (promise->selfstatus.status == BUFFIO_ROUTINE_STATUS_EXECUTING) {
        taskhandle.resume();
      }

      switch (taskhandle.promise().selfstatus.status) {
        /* This case is true when the task want to push some
         * task to the queue and reschedule the current task
         * we can aslo push task buy passing the eventloop
         * instance to the task and the push from there
         * and must be done via eventloop instance that directly
         * associating with queue;
         */
      case BUFFIO_ROUTINE_STATUS_PAUSED:
        queue->pushroutine(promise->pushhandle);

        /* This case is true when the task want to give control
         * back to the eventloop and we reschedule the task to
         * execute the next task in the queue.
         *
         */

      case BUFFIO_ROUTINE_STATUS_YIELD:
        promise->selfstatus.status = BUFFIO_ROUTINE_STATUS_EXECUTING;
        queue->reschedule(taskinfo);
        break;
        /*  This case is true when the task wants to
         *  wait for certain other operations;
         */
      case BUFFIO_ROUTINE_STATUS_WAITING:
        queue->settaskwaiter(
            taskinfo,
            promise->waitingfor); // pushing task to waiting map
        break;
        /* These cases follow the same handling process -
         *
         *  1) BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
         *  2) BUFFIO_ROUTINE_STATUS_ERROR:
         *  3) BUFFIO_ROUTINE_STATUS_DONE:
         *
         */
      case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
      case BUFFIO_ROUTINE_STATUS_ERROR:
      case BUFFIO_ROUTINE_STATUS_DONE:
        auto *handle =
            queue->poptaskwaiter(taskinfo); // pulling out any task awaiter in
                                            // case of taskdone or taskerror

        if (handle != nullptr) {
          // setting the waiter status to executing;
          handle->handle.promise().selfstatus.status =
              BUFFIO_ROUTINE_STATUS_EXECUTING;
          // escalating the task error to the parent if there any error;
          buffioescalatetaskerror(handle, taskinfo);
        }

        // destroying task handle if task errored out and task done
        taskhandle.destroy();
        /*
         * removing the task from execqueue and putting in freequeuelist
         * to reuse the task allocated chunk later and prevent allocation
         * of newer chunk every time.
         */
        queue->poptask(taskinfo);
        break;
      };
    };
    BUFFIO_INFO(" Queue empty: no task to execute ");
    return 0;
  }

  void fireeventloop(enum BUFFIO_EVENTLOOP_TYPE eventlooptype) {
    ieventlooptype = eventlooptype;
    switch (eventlooptype) {
    case BUFFIO_EVENTLOOP_SYNC:
      eventloop(this);
      break;
    case BUFFIO_EVENTLOOP_ASYNC:
      break;
    }
    return;
  };
  buffioinstance() : ieventlooptype(BUFFIO_EVENTLOOP_DOWN) {}
  ~buffioinstance() {
    BUFFIO_INFO("Total executed task : ", iqueue.taskn());
    // clean up-code;
  };

  int push(buffioroutine routine) {
    switch (ieventlooptype) {
    case BUFFIO_EVENTLOOP_DOWN:
    case BUFFIO_EVENTLOOP_SYNC:
      iqueue.pushroutine(routine);
      break;
    case BUFFIO_EVENTLOOP_ASYNC:
      break;
    }
    return 0;
  };

private:
  buffioqueue<buffiotaskinfo, buffioroutine> iqueue;
  enum BUFFIO_EVENTLOOP_TYPE ieventlooptype;
};

#endif
#endif
