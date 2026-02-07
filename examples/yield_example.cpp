#include "buffio/scheduler.hpp"
#include <iostream>

buffio::promise<int> yielder(int id) {

  for (int i = 0; i < 2; i++) {
    std::cout << "yielding back to queue " << id << "i : " << i << std::endl;
    buffioyeild 0; // yields back a int value
  };

  buffioreturn 0;
};
int main() {

  buffio::scheduler scheduler;
  for (int i = 0; i < 100; i++)
    scheduler.push(yielder(i));

  scheduler.run();

  return 0;
};
