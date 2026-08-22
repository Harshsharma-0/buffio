#include "buffio/file.hpp"
#include "buffio/socket.hpp"
#include "buffio/worker.hpp"
#include "buffio/instance.hpp"
#include <iostream>

volatile int he = 10;


buffio::task<size_t> hello(int id) {
  std::cout << "hello world! baby - id " << id << std::endl;
  id += 1;
  co_return 20;
}

std::filesystem::path path = "";
char buffer[1024];

buffio::task<size_t> helloWorld(int id) {
  buffio::File file;
  
  file.fd = 0;
  std::cout<<"[hello world init] "<<std::endl;
  auto reas = co_await file.read(buffer,1024);
  std::cout << "hello world! " << buffer << std::endl;
  auto res  = co_await hello(32);
  std::cout<< "hello world after hello"<<std::endl;
  co_return 0;
};

int main() {
  
  buffio::Worker instance;
  instance.init(4,1024);
  helloWorld(0).schedule(instance);

  for (int i = 0; i < 10; i++) {
    hello(i).schedule(instance);
   };
  
  std::cout<<instance.run()<<std::endl;

  /*
  buffio::Worker worker;
  worker.init(4);
 */
 // std::cout<<sizeof(std::optional<std::variant<long int>>)<<std::endl;
  return 0;
}; 
