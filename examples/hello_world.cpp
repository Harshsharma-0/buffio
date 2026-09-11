#include "buffio/flags.hpp"
#include "buffio/fs.hpp"
#include "buffio/socket.hpp"
#include "buffio/worker.hpp"
#include "buffio/instance.hpp"
#include "buffio/ecode.hpp"
#include <iostream>


const char data[] = "Hello World\n";
char buffer[1024];
buffio::BuffervState iov;

buffio::task<size_t> helloWorld(int id) {

  buffio::File file;
  std::cout<<"[running] "<<std::endl;

  int isOpen = co_await file.Open(BUFFIO_WIDEN("./hello.txt"),
                              B_RDWR | B_CREAT | B_APPEND,0644);
 
 if(isOpen < 0){
     std::cout<<"[error opening reason]"<<buffio::strerror(isOpen)<<std::endl; 
     co_return -1;
  }
  auto[bytesWritten,error] = co_await file.Write((char *)data,
                                            (uint32_t)sizeof(data));

 if(error < 0){
     std::cout<<"[error writing reason]"<<buffio::strerror(error)<<std::endl; 
     co_return -1;
  }
  iov.CreateVec(1);
  iov.MakeEntry(1,buffer,1024);

  std::cout<<"[total writen] "<<bytesWritten<<" "<<sizeof(data)<<std::endl;
 
  // using readAt, as reading soon after writing result's in EOF 
  auto [bytesRead,rerror] = co_await file.ReadvAt(iov,0);
  if(rerror < 0){
     std::cout<<"[error reading reason]"<<buffio::strerror(rerror)<<std::endl; 
     co_return -1;
  }


  std::cout<<"[total read] "<<bytesRead<<std::endl;
    
  for(int i = 0 ; i < bytesRead ; i++){
     std::cout<<buffer[i];
   };
   

  co_return 0;
};


int main() {
  
  buffio::Instance instance;
  int val = instance.init(4,1024);
  if(val < 0){
      std::cout<<"[error init queue] "<<buffio::strerror(val)<<std::endl;
      return -1;
  }
  for(int i = 0; i < 1; i ++)
    helloWorld(i).schedule(instance);

  
  instance.run();
  return 0;
}; 
