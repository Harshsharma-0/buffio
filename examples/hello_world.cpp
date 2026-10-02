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

  buffio::Path dirPath;
  buffio::Path newPath;
  dirPath = "./hell";
  newPath = "./name";

  //int err = co_await buffio::Fs::MkDir(dirPath,0777);
 // std::cout<<"FILE CREATION OK "<<buffio::strerror(err)<<std::endl;

  int err = co_await buffio::Fs::Rename(dirPath,newPath);
  std::cout<<"FILE CREATION OK "<<buffio::strerror(err)<<std::endl;

   
  
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
