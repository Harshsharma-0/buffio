#include "buffio/scheduler.hpp"

#include <iostream>

buffio::promise<int> client() {
  buffio::Fd connectFd;
  struct sockaddr_in addr;

  /*
   * by default MakeFd::socket create a ipv4 socket and on port 8080
   * return negative num on error.
   *
   * here false control whether to bind the socket or not
   */

  if (buffio::MakeFd::socket(connectFd, (sockaddr *)&addr, "127.0.0.1",
                             false) != 0) {
    std::cout << "error" << std::endl;
    buffioreturn 0;
  };

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
    delay.ms = 1000;     // sleep 500ms for server to become up and running;
    __buffioCall(delay); // sleeping to maintain the lifecycle of the fd
  };

  std::cout << "client exiting" << std::endl;
  buffioreturn 0;
};
buffio::promise<int> asyncAcceptEx(int fd, sockaddr_in addr, socklen_t len) {

  std::cout << "connection accepted : fd - " << fd
            << " ip adress : " << inet_ntoa(addr.sin_addr) << std::endl;
  ::close(fd);
  buffioreturn 0;
};
buffio::promise<int> server() {

  buffio::Fd serverFd;
  struct sockaddr_in addr;

  /*
   * by default MakeFd::socket create a ipv4 socket and on port 8080
   * return negative num on error
   */
  if (buffio::MakeFd::socket(serverFd, (sockaddr *)&addr, "127.0.0.1") != 0) {
    std::cout << "error" << std::endl;
    buffioreturn 0;
  };

  /* listen with 2 backlogs
   */

  serverFd.listen(2);
  std::cout << "server listening..." << std::endl;

  __buffioCall(serverFd.asyncAccept(asyncAcceptEx));

  buffio::clockSpec::wait delay;
  delay.ms = -1; // sleep forever or until the timeout

  __buffioCall(delay); // sleeping to maintain the lifecycle of the fd

  buffioreturn 0;
};
int main() {

  buffio::scheduler scheduler;

  scheduler.push(client());
  scheduler.push(server());

  scheduler.run();

  return 0;
};
