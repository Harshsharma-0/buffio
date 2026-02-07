#include "buffio/scheduler.hpp"

#include <iostream>

buffio::promise<int> helloWorld() {

  std::cout << "hello World" << std::endl;
  buffioreturn 0;
};
int main() {

  buffio::scheduler scheduler;
  scheduler.push(helloWorld());
  scheduler.run();

  return 0;
};
