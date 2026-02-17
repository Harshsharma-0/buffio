#include "buffio/fd.hpp"
#include "buffio/scheduler.hpp"
#include <fcntl.h>
#include <iostream>

buffio::promise<int> helloRead() {
  buffio::Fd fd;

  (void)buffio::MakeFd::openFile(fd, "./hello_buffio_test.txt",
                                 O_RDWR | O_FSYNC);
  char *buffer = new char[13];
  __buffioCall(fd.waitRead(buffer, 12));
  std::cout << "[client read] " << buffer << std::endl;
  delete buffer;
  buffioreturn 0;
};

buffio::promise<int> helloWrite() {
  buffio::Fd fd;

  (void)buffio::MakeFd::openFile(fd, "./hello_buffio_test.txt",
                                 O_RDWR | O_TRUNC);
  fd.write("Hello World!", 12);
  std::cout << "[Fd] Write done!" << std::endl;
  buffioreturn 0;
};

int main() {

  buffio::scheduler scheduler;
  scheduler.push(helloWrite());
  scheduler.push(helloRead());
  scheduler.run();

  return 0;
};
