#include "buffio/scheduler.hpp"
#include <iostream>

buffioPromise<int> hello4() {
  std::cout << "helloWorld 4" << std::endl;
  buffioreturn 0;
};
buffioPromise<int> hello() {

  std::cout << "helloWorld" << std::endl;
  buffiowait hello4();
  buffiowait hello4();
  buffioreturn 0;
};
int main() {

  buffio::scheduler scheduler;
  scheduler.push(hello().get());

  scheduler.run();

  return 0;
};
