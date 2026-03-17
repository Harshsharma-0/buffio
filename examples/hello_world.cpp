#include "buffio/scheduler.hpp"

#include <iostream>

buffio::promise<int> helloWorld() {

  std::cout << "hello World" << std::endl;
  buffioreturn 0;
};
int main() {

  buffio::scheduler scheduler;
  scheduler.init();
  scheduler.push(helloWorld());
  scheduler.run();
  scheduler.clean();

  return 0;
};
