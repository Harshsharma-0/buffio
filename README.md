# BUFFIO 
## Async I/O library with c++ coroutine.
Buffio is a personal project to understand async I/O and how it's done. The implementation can contain bugs, critical flaws,  and any type of correction is accepted.
Buffio features:-
  - Tasks based on c++ coroutine
  -  Async handling of socket,file and other types.
  -  Timer
  -  Signal handling

Buffio only provides you with a eventloop and fd operations function, all the low level things like buffer management and other things must be taken care of.


## TODO
 - Add support for file operation.
  
>[!NOTE]
 > - Due of lack to time, the documentation is outdated, and will be updated in future

> Any one who wan't to use buffio, refer to examples, to understand how to use it, and documentation of code are also provided in header file

## USAGE
 - To use in your project add `add_subdirectory(buffio)` and `target_link_libraries(myapp PRIVATE buffio)`  in your CMakeLists.txt

```CMakeLists.txt
  add_subdirectory(buffio)
  target_link_libraries(myapp PRIVATE buffio)
```
   


 ### Quick example:
   ```cpp 
    #include "buffio/scheduler.hpp"
    #include <iostream>
    
    buffio::promise<int> task(int val){
     std::cout<<"Hello World! - "<<val<<std::endl;
     buffioreturn 0;
    }
    int main(){
      buffio::schedular evloop;
      evloop.schedule(task(0));
      evloop.run();
     return 0;
    }
   ```

>[!NOTE]
 For more example refer to the `examples` folder

