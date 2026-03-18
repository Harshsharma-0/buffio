#include "buffio/scheduler.hpp"
#include <cstring>
#include <iostream>
#include <string>

#define SERVER_PORT 8086

buffio::promise<int> asyncReadRoutine(int errorCode, char *buffer, size_t len,
                                      buffio::Fd *fd) {

  if (len != 0)
    std::cout << "[async read] " << buffer << " len : " << len << std::endl;

  int i = 0;
  for (; i != 1;) {
    buffio::clockSpec::wait clk;
    clk.ms = 1000;
    __buffioCall(fd->asyncRead(buffer, 1024, asyncReadRoutine));
    __buffioCall(clk);
    i += 1;
  }
  std::cout << "[async exit] " << std::endl;

  buffioreturn 0;
}
buffio::promise<int> client() {
  buffio::Fd *connectFd = new buffio::Fd;
  struct sockaddr_in addr;

  /*
   * by default MakeFd::socket create a ipv4 socket and on port 8080
   * return negative num on error.
   *
   * here false control whether to bind the socket or not
   */

  if (buffio::MakeFd::socket(*connectFd, (sockaddr *)&addr, "127.0.0.1", false,
                             SERVER_PORT) != 0) {
    std::cout << "error" << std::endl;
    buffioreturn 0;
  };

  std::cout << "connecting fd " << connectFd->getFd() << std::endl;
  // taking 5 attempts to connect to the server
  for (int i = 0; i < 5; i++) {
    __buffioCall(
        connectFd->waitConnect((sockaddr *)&addr, sizeof(sockaddr_in)));
    int error = connectFd->getConnectError();
    if (error == 0) {
      std::cout << "connection successfull" << std::endl;
      break;
    }
    std::cout << "connection unsuccessfull attempting in 1s..." << std::endl;
    buffio::clockSpec::wait delay;
    delay.ms = 1000; // sleep 1s just for delay;
    __buffioCall(delay);
  };

  char *buffer = new char[1024];

  __buffioCall(connectFd->asyncRead(buffer, 1024, asyncReadRoutine));

  buffioreturn 0;
};
buffio::promise<int> asyncAcceptEx(int fd, sockaddr_in addr, socklen_t len) {

  std::cout << "connection accepted : fd - " << fd
            << " ip adress : " << inet_ntoa(addr.sin_addr) << std::endl;

  buffio::Fd clientFd;
  buffio::MakeFd::mkFdSock(clientFd, fd, (sockaddr &)addr);

  char buffer[100];
  memcpy(buffer, "echo - 0 echo - 1", 18);
  int i = 0;
  for (;;) {
    buffio::clockSpec::wait clk;
    clk.ms = 1000;
    __buffioCall(clk);
    __buffioCall(clientFd.waitWrite(buffer, strlen(buffer) + 1));
    buffer[7] = 'A' + i;
    i + 1;
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
  delay.ms = -1; // sleep forever or until the timeout

  __buffioCall(delay); // sleeping to maintain the lifecycle of the fd

  buffioreturn 0;
};
int main() {

  buffio::scheduler scheduler(2);

  scheduler.push(client());
  scheduler.push(server());

  scheduler.run();
  scheduler.clean();

  return 0;
};
