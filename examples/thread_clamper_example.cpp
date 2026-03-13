#include "buffio/fiber.hpp"
#include "buffio/scheduler.hpp"

buffio::promise<int> clampedTask1(){
  std::cout << "hello World Clamper" << std::endl;
  buffioreturn 0;

};
buffio::promise<int> clampedTask() {
  std::cout << "hello World!" << std::endl;
  __buffioCall(buffio::fiber::clamper(clampedTask1()).clamp());
  buffioreturn 0;
};
int main() {

  buffio::scheduler scheduler(2);
  scheduler.push(clampedTask());
  scheduler.run();
  scheduler.clean();
  return 0;
};
