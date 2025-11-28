#ifndef __BUFFIO_QUEUE_HPP__
#define __BUFFIO_QUEUE_HPP__

#include <unordered_map>

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
    if (head = head->next) {
      delete head;
      return;
    }

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
    task->next = nullptr;
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

    // removing if entry is greater than 1
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

#endif
