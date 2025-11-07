#include <iostream>
#define BUFFIO_IMPLEMENTATION
#define BUFFIO_DEBUG
#include "./buffio.hpp"


buffioroutine clientcall(){
 std::cout<<"hello client2"<<std::endl;
 co_return 0;
};

buffioroutine clienthandler(clientinfo info){

  std::cout<<"hello client : "<<info.address<<":"<<info.portnumber<<std::endl;
  iowait clientcall();
  std::cout<<"hello client exit"<<std::endl;
  ioreturn 0;
};

int main(){
  
  buffioinfo serverinfo = {
   .address = "127.0.0.1",
   .portnumber = 8081,
   .listenbacklog = 10,
   .capacity = 10,
   .reserve = 10,
   .socktype = BUFFIO_SOCK_TCP,
   .sockfamily = BUFFIO_FAMILY_IPV4
  };

  buffio::buffsocket server(serverinfo);
  server.clienthandler = clienthandler;
  
  
  buffio::instance runner;
  runner.fireeventloop(server); 
  

  return 0;
 };
