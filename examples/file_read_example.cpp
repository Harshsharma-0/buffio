#include "buffio/fd.hpp"
#include "buffio/scheduler.hpp"
#include <fcntl.h>
#include <iostream>

buffio::promise helloRead() {
  buffio::Fd fd;

  (void)buffio::MakeFd::openFile(fd, "./hello_buffio_test.txt",
                                 O_RDWR  | O_FSYNC);
  char *buffer = new char[13];
  __buffioCall(fd.waitRead(buffer, 12));
  std::cout << "[client read] " << buffer << std::endl;
  buffio::clockSpec::wait dlay;
  
  delete []buffer;
  buffioreturn 0;
};

buffio::promise helloWrite() {
  buffio::Fd fd;

  (void)buffio::MakeFd::openFile(fd, "./hello_buffio_test.txt",
                                 O_RDWR | O_TRUNC | O_CREAT);
  fd.write("Hello World!", 12);
  std::cout << "[Fd] Write done!" << std::endl;
  buffioreturn 0;
};

int main() {

  buffio::scheduler scheduler;
  scheduler.init();
  scheduler.push(helloWrite());
  scheduler.push(helloRead());
  auto now = std::chrono::steady_clock::now();
  scheduler.run();
  auto end = std::chrono::steady_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - now);
  auto diffms = std::chrono::duration_cast<std::chrono::milliseconds>(end - now);

  std::cout<<"[process takes time to run] us: "<<diff.count()<<" in ms : "<<diffms.count()<<std::endl;

  scheduler.clean();

  return 0;
};
