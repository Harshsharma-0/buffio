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

buffio::task<size_t> helloWorld(int id) {
  std::cout << "hello world! " << id << std::endl;
 // BFILE file;
 // BFRWOPT ret = co_await buffio::readFile{file,nullptr,100};
  co_return 0;
};

int main() {
  buffio::instance instance;
  instance.init(4,1024);
  for (int i = 0; i < 64; i++) {
    helloWorld(i).schedule(instance);
  };
  
  std::cout<<instance.run()<<std::endl;

  /*
  buffio::Worker worker;
  worker.init(4);
 */

  return 0;
}; 
