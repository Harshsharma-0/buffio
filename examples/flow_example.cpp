#include <iostream>
#include "buffio/scheduler.hpp"

int main(){
  buffio::scheduler evloop;
  evloop.init();
  evloop.run();
  evloop.clean();
  return 0;
};
