#include "buffio/flags.hpp"
#include "buffio/fs.hpp"
#include "buffio/socket.hpp"
#include "buffio/worker.hpp"
#include "buffio/instance.hpp"
#include "buffio/ecode.hpp"
#include <iostream>





std::filesystem::path path = "";
const char data[] = "Hello harsh shama \n";
char buffer[1024];

buffio::task<size_t> helloWorld(int id) {
  buffio::File file;
  std::cout<<"[hello world] "<<std::endl;  
  int isOpen = co_await file.Open(BUFFIO_WIDEN("./hello.txt"),B_RDWR | B_CREAT | B_APPEND,0644);
  std::cout<<"[file] "<<isOpen<<std::endl;

  auto reas = co_await file.Write((char *)data,(uint32_t)sizeof(data));
  if(reas < 0)
     buffio::strerror(reas); 
  //assert(reas == sizeof(data));
 // assert(reas == sizeof(data));
  std::cout<<"[total writen] "<<reas<<" "<<sizeof(data)<<std::endl;
 
  buffio::BuffervState iovec;
  iovec.CreateVec(1);
  iovec.MakeEntry(1,buffer,sizeof(buffer));

  
  auto res = co_await file.Read(buffer,(uint32_t)sizeof(buffer));
  std::cout<<"[total read] "<<res<<std::endl;
  
  for(int i = 0 ; i < res ; i++){
     std::cout<<buffer[i];
   };
   

  co_return 0;
};


int main() {
  
  buffio::Instance instance;
  int val = instance.init(4,1024);
  if(val < 0){
   // std::cout<<"[initlisation failed] "<<val<<std::endl;
  };

  for(int i = 0; i < 1; i ++)
    helloWorld(i).schedule(instance);

  
 std::cout<<instance.run()<<std::endl; 
  /*
  buffio::Worker worker;
  worker.init(4);
 */
 // std::cout<<sizeof(std::optional<std::variant<long int>>)<<std::endl;
  return 0;
}; 
