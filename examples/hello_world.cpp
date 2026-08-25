#include "buffio/flags.hpp"
#include "buffio/fs.hpp"
#include "buffio/socket.hpp"
#include "buffio/worker.hpp"
#include "buffio/instance.hpp"
#include <iostream>





std::filesystem::path path = "";
const char data[] = "Hello this is the buffio test!";
char buffer[1024];

buffio::task<size_t> helloWorld(int id) {
  buffio::File file;
 

  const char *loc = "./hello";
  int isOpen = co_await file.Open("./test.txt",B_RDWR | B_CREAT,0644);
  std::cout<<"[file] "<<isOpen<<std::endl;

  auto reas = co_await file.Write((char *)data,(uint32_t)sizeof(data));
  assert(reas == sizeof(data));
  std::cout<<"[total writen] "<<reas<<" "<<sizeof(data)<<std::endl;
 
  buffio::BuffervState iovec;
  iovec.CreateVec(1);
  iovec.MakeEntry(1,buffer,(uint32_t)sizeof(data));

  auto res = co_await file.Readv(iovec);
  buffer[sizeof(data)] = '\0';
  std::cout<<buffer<<std::endl;

  co_return 0;
};

int main() {
  
  buffio::Instance instance;
  instance.init(4);
  helloWorld(0).schedule(instance);

  
  std::cout<<instance.run()<<std::endl;

  /*
  buffio::Worker worker;
  worker.init(4);
 */
 // std::cout<<sizeof(std::optional<std::variant<long int>>)<<std::endl;
  return 0;
}; 
