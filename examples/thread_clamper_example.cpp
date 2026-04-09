#include "buffio/fiber.hpp"
#include "buffio/scheduler.hpp"


buffio::promise clampedSelf() {
  std::cout << "hello World!" << std::endl;
  std::cout << "hello World! 7" << std::endl;
  std::cout << "hello World! 7" << std::endl;

  buffioreturn 0;
};

int main() {

  buffio::scheduler scheduler(2);
  scheduler.push(clampedSelf());
  scheduler.run();
  scheduler.clean();
  return 0;
};
