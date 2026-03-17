#include "buffio/promise.hpp"
#include "buffio/scheduler.hpp"
#include <fcntl.h>
#include <iostream>

#define SERVER_PORT 8084
buffio::Fd fileFd;
int j = 0;
buffio::promise<int> onWrite(int opError, char *buffer, size_t len,
                             buffio::Fd *fd) {
  std::cout << "file write done : " << j << std::endl;
  j += 1;

  buffioreturn 0;
};
buffio::promise<int> client() {
  buffio::Fd connectFd;
  struct sockaddr_in addr;

  if (buffio::MakeFd::socket(connectFd, (sockaddr *)&addr, "127.0.0.1", false,
                             SERVER_PORT) != 0) {
    std::cout << "error" << std::endl;
    buffioreturn 0;
  };

  std::cout << "connecting fd " << connectFd.getFd() << std::endl;
  // taking 5 attempts to connect to the server
  for (int i = 0; i < 5; i++) {
    __buffioCall(connectFd.waitConnect((sockaddr *)&addr, sizeof(sockaddr_in)));
    int error = connectFd.getConnectError();
    if (error == 0) {
      std::cout << "connection successfull" << std::endl;
      break;
    }
    std::cout << "connection unsuccessfull attempting in 1s..." << std::endl;
    buffio::clockSpec::wait delay;
    delay.ms = 1000; // sleep 1s just for delay;
    __buffioCall(delay);
  };

  char buffer[1024];

  for (int i = 0; i < 10; i++) {
    buffio::clockSpec::wait clk;
    clk.ms = 1000;

    __buffioCall(connectFd.waitRead(buffer, 1024));
    __buffioCall(fileFd.asyncWrite(buffer, 6, onWrite));
    std::cout << "client read : " << buffer << i << std::endl;
    __buffioCall(clk);
  }
  std::cout << "client exit" << std::endl;
  buffioreturn 0;
};
buffio::promise<int> asyncAcceptEx(int fd, sockaddr_in addr, socklen_t len) {

  std::cout << "connection accepted : fd - " << fd
            << " ip adress : " << inet_ntoa(addr.sin_addr) << std::endl;

  buffio::Fd clientFd;
  buffio::MakeFd::mkFdSock(clientFd, fd, (sockaddr &)addr);

  const char *buffer = "echo";

  for (int i = 0; i < 10; i++) {
    buffio::clockSpec::wait clk;
    clk.ms = 1000;
    __buffioCall(clk);
    __buffioCall(clientFd.waitWrite((char *)buffer, strlen(buffer) + 1));
  }
  buffioreturn 0;
};
buffio::promise<int> server() {

  buffio::Fd serverFd;
  struct sockaddr_in addr;

  /*
   * by default MakeFd::socket create a ipv4 socket and on port 8080
   * return negative num on error
   */
  if (buffio::MakeFd::socket(serverFd, (sockaddr *)&addr, "127.0.0.1", true,
                             SERVER_PORT) != 0) {
    std::cout << "error" << std::endl;
    assert(false);
    buffioreturn 0;
  };

  /* listen with 2 backlogs
   */

  serverFd.listen(2);
  std::cout << "server listening... " << serverFd.getFd() << std::endl;

  __buffioCall(serverFd.asyncAccept(asyncAcceptEx));

  buffio::clockSpec::wait delay;
  delay.ms = 1000; // sleep forever or until the timeout

  __buffioCall(delay); // sleeping to maintain the lifecycle of the fd
  std::cout << "main exit" << std::endl;
  serverFd.asyncAccpetDone();
  buffioreturn 0;
};

int main() {
  (void)buffio::MakeFd::openFile(fileFd, "./echo_write.txt",
                                 O_RDWR | O_FSYNC | O_CREAT);

  buffio::scheduler scheduler;
  scheduler.init();
  scheduler.push(server());
  scheduler.push(client());
  scheduler.run();

  scheduler.clean();

  return 0;
};
