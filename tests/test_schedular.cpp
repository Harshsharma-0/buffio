#include "buffio/scheduler.hpp"

buffio::promise<int> hello() {

  std::cout << "hello World" << std::endl;
  buffioreturn 0;
};
int main() {

  buffio::scheduler scheduler;
  scheduler.push(hello());
  scheduler.run();

  return 0;
};
