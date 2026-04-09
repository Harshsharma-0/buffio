#include "buffio/scheduler.hpp"
#include "buffio/container.hpp"
#include <iostream>

buffio::promise helloWorld() {

  std::cout << "hello World" << std::endl;
  buffioreturn 0;
};
int main() {

  
  buffio::scheduler scheduler;
  scheduler.init();
  scheduler.push(helloWorld());
  scheduler.run();
 // scheduler.clean();
 // uni.destroy(uni.storage);
  return 0;
};
