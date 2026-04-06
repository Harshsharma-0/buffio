#include <iostream>
#include "buffio/scheduler.hpp"

int main(){
  buffio::scheduler evloop;

  evloop.init();

  auto cbv = [](void *data) -> void {
      std::cout<<"helloWorld"<<std::endl;
      return ;
  };
  auto cb = [](void *data) -> void {
      std::cout<<"after "<<std::endl;
      return ;
  };


  buffio::makeFlow().make(nullptr,cbv).then(cb,nullptr).flowDone();

  evloop.run();
  evloop.clean();
  return 0;
};
