#include "buffio/fiber.hpp"

namespace buffio{
void makeFlow::flowDone(){
  buffio::fiber::flowQueue->push(head);
  head = nullptr;
};
}
