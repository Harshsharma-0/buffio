#include "buffio/container.hpp"
#include "buffio/fiber.hpp"

namespace buffio {

void makeContainer::routine(buffio::promise &_promise, buffio::container &tmp) {

  static_assert(sizeof(buffio::promise) <= CONTAINER_STORAGE_SIZE);
  new (tmp.storage) buffio::promise(std::move(_promise));
  tmp.run = buffio::promise::run;
  tmp.destroy = buffio::promise::destroy;
  return;
};
void makeContainer::function(containerCallback callback, void *data,
                             buffio::container &tmp){
 static_assert(sizeof(data) <= CONTAINER_STORAGE_SIZE);
 auto *then = new (tmp.storage) buffio::functionInfo;
 then->data = data;
 then->run = callback;
 tmp.run = [](void *data){
   auto *ptr = (buffio::functionInfo*)data;
   ptr->run(nullptr);
   buffio::fiber::queue->pop();
 };
 
};

}; // namespace buffio
