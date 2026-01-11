# BUFFIO 
## Async I/O library with c++ coroutine.
   - Do async I/O in a organised way and clear way, no callback needed just   cpp coroutines and buffio eventloop to manage it all. 
    - Buffio is collection of seperate `.hpp` files that when combines create a performancefull system that is modular and configurable.
    - Each module of the buffio can be used seperately or with each other,
      > **Buffio core**
      >  1. `buffio.hpp ` just a wrapper of all buffio modules.
          2. `buffioschedular` schedular for buffio coroutines and the eventloop.
          3. `buffiosockbroker` socket I/O request watcher and worker thread manager.
          4. `buffiothread` provide class wrapper to create thread and manage them.
          5. `buffioqueue` proveide a fixed size queue that is not thread safe.
          6. `buffiolfqueue` provedes MPMC lockfree queue, and is thread safe.
          7. `buffiolog` provides logging function.
          8. `buffiopromise` provides necessary defination to create buffiotask(coroutine) and    use.
          9. `buffiosock` provides abstraction to create and manage network sockets

>[!NOTE]
> Defination Macro `BUFFIO_IMPLEMENTATION` must  be used on top of the file where you   want to use buffio only applied for `buffio.hpp`. 
> - BUFFIO_DEBUF_BUILD macro is used to enable debug logs that are used while debugging
> - BUFFIO_LOG_LOG is used to enable the loglevel=LOG  that can be viewed.

 ### Quick example:
   ```cpp 
    #define BUFFIO_IMPLEMENTATION
    #define BUFFIO_DEGUB_BUILD
    #define BUFFIO_LOG_LOG
    #include "./buffio.hpp"
    #include <iostream>
    
    buffiopromise task(int val){
     std::cout<<"Hello World! - "<<val<<std::endl;
     buffioreturn 0;
    }
    int main(){
      buffioschedular evloop;
      evloop.schedule(task(0));
      evloop.run();
     return 0;
    }
   ```


