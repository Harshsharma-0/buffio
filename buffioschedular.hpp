#ifndef __BUFFIO_SCHEDULAR__
#define __BUFFIO_SCHEDULAR__


#define buffiotask_read_mask 1

// set when the user want to request a write
#define buffiotask_write_mask 1 << 1 

// set when the user have submitted the fd for polling
#define buffiotask_io_poller 1 << 2 

// use this to if want to wakeup the routine after the write is done
#define buffiotask_notify_back 1 << 3 

// use this to directly write content of the received buffer to any fd or buffer
#define buffiotask_recv_write 1 << 4

#if !defined(BUFFIO_IMPLEMENTATION)
   #include "buffiopromsie.hpp"
   #include "buffiosockbroker.hpp"
#endif


// when value of mask < 0, all the read and write request are done
// when value of mask == 0, the task has not registered any r/w
// user can also make io_request with the fd directly without the fd



/* bound to buffiotaskinfo;
 * create a circular buffer of the task using linked list.
 */

class buffioschedular{
  struct __schedinternal{
   buffiotaskinfo fiber; // check buffiopromise for this struct defination 
   struct __schedinternal *waiters;
   struct __schedinternal *wnxt; // contains the next member of the waiter tree;
   size_t waitercount; // the waiter are popped using this count, it should be taken care of
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
     brk->waiters = nullptr;
     return scheduleptr(brk);
   };

   void run(){
    while(active > 0){

    if(cursor == nullptr) return;
     buffioroutine tsk = cursor->fiber.task;
     buffiopromise *promise = &tsk.promise();
     struct __schedinternal *cur = cursor;

     if(promise->selfstatus.status == BUFFIO_ROUTINE_STATUS_EXECUTING)
                        tsk.resume();


      switch(promise->selfstatus.status){
       case BUFFIO_ROUTINE_STATUS_EXECUTING:
       case BUFFIO_ROUTINE_STATUS_YIELD:{
             cursor = cursor->next;
             promise->setstatus(BUFFIO_ROUTINE_STATUS_EXECUTING);

       }break;
       case BUFFIO_ROUTINE_STATUS_WAITING_IO:{}break; //case used to transfer iorequest to the appropritate worker
       case BUFFIO_ROUTINE_STATUS_PAUSED:{} break;
       case BUFFIO_ROUTINE_STATUS_WAITING:{ 
        struct __schedinternal *tsk = schedmem.getfrag();
        tsk->fiber.task = promise->waitingfor;
        tsk->waitercount += 1;
        pushwaiter(tsk,cursor);
        replacecursor(tsk);
        cursor = cursor->next;        
       } break;
       case BUFFIO_ROUTINE_STATUS_PUSH_TASK:{}break;
       case BUFFIO_ROUTINE_STATUS_UNHANDLED_EXCEPTION:
       case BUFFIO_ROUTINE_STATUS_ERROR:
       case BUFFIO_ROUTINE_STATUS_DONE:{
        if(cursor->waiters != nullptr){
           wakeup(cur,cursor->waitercount);
           cursor->waitercount = 0;
        }  
        cursor = cur->next;
        poptask(cur);
        }break;
     }
    }
  };
   
private:
 
  void replacecursor(struct __schedinternal *from){

    cursor->prev->next = from;
    cursor->next->prev = from;
    from->next = cursor->next;
    from->prev = cursor->prev;
    cursor->next = cursor->prev = nullptr;
    cursor = from;

    if(from == head) head = from;
    if(from == tail) tail = from;

  }

  int scheduleptr(struct __schedinternal *brk){
     active += 1;
       //the queue is empty
     if(head == nullptr){
      head = tail = brk;
      head->prev = head->next = brk; //cyclic chain
      cursor = head;
      return 0;
     }

     //cyclic chain of one element;
     if(head == tail){
      head->prev = brk;
      head->next = brk;      
      brk->prev = brk->next = tail;
      tail = brk;
      return 0;
     }

     //after checking both we now schedule w.r.t to the cursor of the schedular;
     cursor->prev->next = brk; // marking brk as next for the node before the cursor
     brk->prev = cursor->prev; // makring brk previous to the cursor previous
     brk->next = cursor; // marking brk next to cursor
     cursor->prev = brk; // marking cursor prev to brk;
     return 0;
   } 

  // call after checking you have a valid pointer, pass the waiters field for the structure
  void wakeup(struct __schedinternal *parent,size_t waitercount){
     struct __schedinternal *waitinglist = parent->waiters;
     buffiopromisestatus tmppromise = parent->fiber.task.promise().selfstatus;

    for(size_t i = 0; i < waitercount;i--){
      buffiopromise *pro = &waitinglist->fiber.task.promise();
      pro->setstatus(BUFFIO_ROUTINE_STATUS_EXECUTING);
      pro->childstatus = tmppromise;
      scheduleptr(waitinglist);
      waitinglist = waitinglist->wnxt;
     }
    return;
  }

  // pass pointer to the raw task structure
  void pushwaiter(struct __schedinternal *task,struct __schedinternal *wtsk){
    if(task->waiters == nullptr){
       wtsk->wnxt = nullptr;
       task->waiters = wtsk;
       task->waitercount += 1;
       return;
    }
    wtsk->wnxt = task->waiters;
    task->waiters = wtsk;
    task->waitercount += 1;
  }
  void poptask(struct __schedinternal *which){

    if(active == 1){
       which->fiber.task.destroy();
       head = tail = nullptr;
       schedmem.reclaim(which);
       cursor = nullptr;
       active -= 1;
       return;
    }
      which->next->prev = which->prev;
      which->prev->next = which->next;
      schedmem.reclaim(which);
      which = which->next;

     if(which == head) head = which;
     if(which == tail) tail = which;
     active -= 1;
     return;
  }

 buffiomemory <__schedinternal>schedmem;
 size_t capinit;
 size_t active;
 struct __schedinternal *head;
 struct __schedinternal *tail;
 struct __schedinternal *cursor;
 buffiosockbroker iopooler; // used to transfer I/O request to the worker thread;
};
#endif
