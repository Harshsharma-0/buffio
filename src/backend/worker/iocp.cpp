#include "buffio/worker.hpp"

//DONE : add signal works
void buffio::Worker::signalLoop(buffio_fd fd,LoopStatusCode code){
  PostQueuedCompletionStatus(fd,0,(ULONG_PTR)code,NULL);
};

//DONE : add init code for IOCP
int buffio::Worker::init_poller(unsigned int order) { 
    HANDLE iocph = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);
    if(iocph == INVALID_HANDLE_VALUE) 
      return buffio::error_from_os(GetLastError());
    state.event.evfd = iocph;
    state.event.sigfd = iocph;
    return 0;
};

//DONE : add wait event loop code
int buffio::Worker::wait_event() {
   HANDLE iocph = state.event.evfd;
   OVERLAPPED_ENTRY events[1024];
   ULONG event_size = 1024;
   ULONG event_done = 0;

   DWORD timeout = state.task_queue.empty() && state.io.pending != 0 ? INFINITE : 0;
   if (timeout != 0)
     state.control.store(buffio::LoopStatusCode::inactive,
                        std::memory_order_release);

   BOOL val = GetQueuedCompletionStatusEx(iocph,events,event_size,&event_done,timeout,FALSE);
   if(!val) return -1;

   if ((int)timeout != 0)
     state.control.store(buffio::LoopStatusCode::active,
                        std::memory_order_release);
                        
   for(ULONG i = 0; i < event_done; i++){
     if(events[i].lpCompletionKey == (ULONG)buffio::LoopStatusCode::event_wake)
           flush_io_completed(64);
   };
     return 0;
};
