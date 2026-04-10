#include "buffio/scheduler.hpp"
#include <iostream>

buffio::promise yielder(int id) {

  for (int i = 0; i < 100000; i++) {
    buffioyeild i; // yields back a int value
  };

  buffioreturn 0;
};

void hello(void *data){
  for(int i = 0; i < 100000;  i++){
    };
}
int main() {

  buffio::scheduler scheduler;
  scheduler.init();

  for(int i = 0; i < 10000; i++){
  scheduler.push(hello,nullptr);
  scheduler.push(yielder(0));
  }
  scheduler.run();  
  scheduler.clean();

  return 0;
};
