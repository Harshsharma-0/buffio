#ifndef __BUFFIO_QUEUE_HPP__
#define __BUFFIO_QUEUE_HPP__

#include <unordered_map>


template<typename X, typename Y>
  class waitingmap{
   private:
   std::unordered_map<X,Y> waitingmap;
   public:
   void push(X id , Y thing){waitingmap[id] = thing;};
   Y pop(X id){
     auto res = waitingmap.find(id);
     if(res != waitingmap.end()) return res->second; 
     return {0};
   };
};


template <typename T> class buffioqueue {
  struct buffiochain{
    T data;
    struct buffiochain *next;
    struct buffiochain *prev;
   };

public:
  buffioqueue()
      : queuehead(nullptr), queuetail(nullptr), freehead(nullptr),
        freetail(nullptr),tmpcount(0),totalcount(0)

  {
    queueerror = BUFFIO_QUEUE_STATUS_EMPTY;
   };
  void setdefault(T def){defaultvalue = def;}
  ~buffioqueue() {
    buffioclean(queuehead);
    buffioclean(freehead);
  };

  void reserve(size_t value){
   if(value != 0)
      for(size_t i = 0; i < value; i++){
        buffiochain *tmp = new buffiochain;
        tmp->data = defaultvalue;
        pushptr(tmp,&freehead,&freetail);
      }

  };
  void push(T data) {
    struct buffiochain *tmproutine = popfreequeue();
    tmproutine->data = data;
    pushptr(tmproutine, &queuehead, &queuetail);
    tmpcount += 1;
    totalcount++;
    };

 T *pushandget(T data){
    struct buffiochain *tmproutine = popfreequeue();
    tmproutine->data = data;
    pushptr(tmproutine, &queuehead, &queuetail);
    tmpcount += 1;
    totalcount++;
    return &tmproutine->data;
   }

  // only to use to mark a ptr get by pushandget() to free queue
  void popget(void *ptr){
   if(ptr == nullptr) return;
   struct buffiochain *tmp = (struct buffiochain*)ptr;
   eraseptr(tmp, &queuehead, &queuetail);
   pushptr(tmp, &freehead, &freetail);
   tmp->data = defaultvalue;
    std::cout<<"pushing back"<<std::endl;
  };

  T pop(){
    struct buffiochain *t = queuehead; 
    if(t == nullptr) return NULL;
    eraseptr(t, &queuehead, &queuetail);
    pushptr(t, &freehead, &freetail);
    tmpcount -= 1;
    T data = t->data;
    t->data = defaultvalue;
    return data;
  }
  
  
  bool empty() { return (tmpcount == 0); }
  size_t queuen() { return tmpcount; };
  int getqueuerrror() { return queueerror; }

private:
 
  void buffioclean(struct buffiochain *head) {
    if (head == nullptr)
      return;
    if (head = head->next) {
      delete head;
      return;
    }

    struct buffiochain *tmp = head;
    while (tmp != nullptr) {
      tmp = head->next;
      delete head;
      head = tmp;
    }
  };

  // checkfor nullptr task before entering this function;
  void pushptr(struct buffiochain *task, struct buffiochain **head, struct buffiochain **tail) {
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
 
  void eraseptr(struct buffiochain *task, struct buffiochain **head, struct buffiochain **tail) {
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

 struct buffiochain *popfreequeue() {
    if (freehead == nullptr) {
    struct buffiochain *t = new struct buffiochain;
      t->next = nullptr;
      t->prev = nullptr;
      std::cout<<"allocating new"<<std::endl;
      return t;
    };
    struct buffiochain *ret = freehead;
    std::cout<<"reusing"<<std::endl;
    eraseptr(ret, &freehead, &freetail);
    return ret;
  }

  struct buffiochain *queuehead, *queuetail;
  struct buffiochain *freehead, *freetail;

  int queueerror;
  size_t tmpcount;
  size_t totalcount;
  T defaultvalue;
};

#endif
