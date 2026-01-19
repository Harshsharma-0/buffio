#ifndef __BUFFIO_SCHEDULAR__
#define __BUFFIO_SCHEDULAR__

/*
* Error codes range reserved for buffioschedular
*  [4000 - 5500]
*  4000 <= errorcode <= 5500
*/

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
   int init(struct buffiosockbrokerconf _cfg,size_t _capinitial){
     int tmp_err = init(_capinitial);
     if(tmp_err < 0) return tmp_err;

     sb_error poll_err = iopoller.configure(_cfg);
     if(poll_err != sb_error::none) return static_cast<int>(poll_err);
     sb_error ini_err = iopoller.init();
     if(ini_err != sb_error::none) return static_cast<int>(ini_err);
     return 0;
  }
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

     if(promise->selfstatus.status == buffio_routine_status::executing)
                        tsk.resume();


      switch(promise->selfstatus.status){
        case buffio_routine_status::executing:
        cursor = cursor->next;
        break;
        case buffio_routine_status::yield:{
             cursor = cursor->next;
             promise->setstatus(buffio_routine_status::executing);

       }break;
       case buffio_routine_status::waiting_io: {} break;
       case buffio_routine_status::paused: {} break;
       case buffio_routine_status::waiting:{
        struct __schedinternal *tsk = schedmem.getfrag();
        tsk->fiber.task = promise->waitingfor;
        tsk->waitercount += 1;
        pushwaiter(tsk,cursor);
        replacecursor(tsk);
        cursor = cursor->next;        
       } break;
       case buffio_routine_status::push_task:{}break;     
       case buffio_routine_status::unhandled_exception:
       case buffio_routine_status::error:
       case buffio_routine_status::done:{
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
       //the queue is empty
     if(head == nullptr){
      head = tail = brk;
      head->prev = head->next = brk; //cyclic chain
      cursor = head;
      active += 1;

      return 0;
     }

     //cyclic chain of one element;
     if(head == tail){
      head->prev = brk;
      head->next = brk;      
      brk->prev = brk->next = tail;
      tail = brk;
      active += 1;
      return 0;
     }

     //after checking both we now schedule w.r.t to the cursor of the schedular;
     cursor->prev->next = brk; // marking brk as next for the node before the cursor
     brk->prev = cursor->prev; // makring brk previous to the cursor previous
     brk->next = cursor; // marking brk next to cursor
     cursor->prev = brk; // marking cursor prev to brk;
     active += 1;

     return 0;
   } 

  // call after checking you have a valid pointer, pass the waiters field for the structure
  void wakeup(struct __schedinternal *parent,size_t waitercount){
     struct __schedinternal *waitinglist = parent->waiters;
     buffiopromisestatus tmppromise = parent->fiber.task.promise().selfstatus;

    for(size_t i = 0; i < waitercount;i--){
      buffiopromise *pro = &waitinglist->fiber.task.promise();
      pro->setstatus(buffio_routine_status::executing);
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

    if(head == tail){
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
 buffiosockbroker iopoller; // used to transfer I/O request to the worker thread;
};
#endif
