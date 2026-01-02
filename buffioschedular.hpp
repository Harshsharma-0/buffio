#ifndef __BUFFIO_SCHEDULAR__
#define __BUFFIO_SCHEDULAR__


struct buffiotaskinfo{
  size_t id;
  buffioroutine task;
  buffiosbrokerinfo sockinfo;
};

/* bound to buffiotaskinfo;
 * create a circular buffer of the task using linked list.
 */

class buffioschedular{
  struct __schedinternal{
   buffiotaskinfo fiber;
   __schedinternal *next;
   __schedinternal *prev;
  };

public:
  // constructor of the schedular.
  buffioschedular(size_t _capinitial):
           head(nullptr),tail(nullptr),
           cursor(nullptr),capinit(0),active(0){
               init(_capinitial); 
          };
  // constructor overload.
  buffioschedular():head(nullptr),tail(nullptr),
           cursor(nullptr),capinit(0),active(0){};

  // initliser of the schedular
  int init(size_t _capinitial){
    if(capinit != 0) return -1;
    capinit = _capinitial;
    return schedmem.init(capinit);
  }
  ~buffioschedular(){}

   int schedule(buffioroutine routine){
     struct __schedinternal *brk = schedmem.getfrag();
     brk->fiber.task = routine;
     active += 1;
     if(head == nullptr){ 
      head = brk; tail = brk; cursor = head;
      head->next = head->prev = tail;
      return 0;
     }

     // size of the queue is one;
     if(head == tail){
       brk->next = brk->prev = tail;
       head->next = head->prev = brk;
       head = brk;
       return 0;
     }
     cursor->prev->next = brk;
     brk->next = cursor;
     brk->prev = cursor->prev;
     cursor->prev = brk;

     return 0;
   };

   void execnext(){
    if(cursor == nullptr) return;
    cursor->fiber.task.resume();
    cursor = cursor->next;
   };

   void popcursor(){
  
    if(active > 0) active -= 1;
    if(active == 1){
       head->fiber.task.destroy();
       head = tail = nullptr;
       schedmem.reclaim(cursor);
       cursor = nullptr;
       return;
    }
      cursor->next->prev = cursor->prev;
      cursor->prev->next = cursor->next;
      schedmem.reclaim(cursor);
      cursor = cursor->next;

     if(cursor == head) head = cursor;
     if(cursor == tail) tail = cursor;
  }
   
private:
 
 buffiomemory <__schedinternal>schedmem;
 size_t capinit;
 size_t active;
 struct __schedinternal *head;
 struct __schedinternal *tail;
 struct __schedinternal *cursor;
  
};
#endif
