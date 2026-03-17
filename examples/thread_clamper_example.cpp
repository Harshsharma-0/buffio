#include "buffio/fiber.hpp"
#include "buffio/scheduler.hpp"

buffio::promise<int> clampedTask2() {
  std::cout << "hello World Clamper 2" << std::endl;
  buffioreturn 0;
};

buffio::promise<int> clampedTask1() {
  std::cout << "hello World Clamper" << std::endl;
  buffioreturn 0;
};
buffio::promise<int> clampedTask() {
  std::cout << "hello World!" << std::endl;
  __buffioCall(buffio::fiber::clamper(clampedTask1()).clamp(clampedTask2()));
  buffioreturn 0;
};

buffio::promise<int> clampedSelf() {
  std::cout << "hello World!" << std::endl;
  __buffioCall(buffio::fiber::clamper().sclamp(clampedTask()));
  std::cout << "hello World! 7" << std::endl;

  buffioreturn 0;
};

int main() {

 buffio::scheduler scheduler(2);
// scheduler.push(clampedTask());
 scheduler.push(clampedSelf());

  scheduler.run();
  scheduler.clean();
  return 0;
};
