#include "buffio/container.hpp"
#include "buffio/fiber.hpp"

namespace buffio{

void makeContainer::makeFromRoutine(buffio::promise &_promise, buffio::container &tmp){

    static_assert(sizeof(buffio::promise) <= CONTAINER_STORAGE_SIZE);
    new (tmp.storage) buffio::promise(std::move(_promise));
    tmp.run = buffio::promise::run;
    tmp.destroy = buffio::promise::destroy;
    tmp.setStatus = buffio::promise::setStatus;
    tmp.getHeader = buffio::promise::getHeader;
    tmp.getStatus = buffio::promise::checkStatus;
    return;

};
};
