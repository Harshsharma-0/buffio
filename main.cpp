#include <iostream>
#define BUFFIO_IMPLEMENTATION
#define BUFFIO_DEBUG_BUILD
#define BUFFIO_LOG_ERROR
#define BUFFIO_LOG_LOG
#define BUFFIO_LOG_TRACE

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
   .address = "127.0.0.0",
   .portnumber = 8081,
   .listenbacklog = 10,
   .socktype = BUFFIO_SOCK_TCP,
   .sockfamily = BUFFIO_FAMILY_IPV4
  };

  buffioqueuepolicy queuepolicy = {
   .overflowpolicy = OVERFLOWPOLICY_NONE,
   .threadpolicy = THREAD_POLICY_NONE,
   .chunkallocationpolicy = CHUNKALLOCATION_POLICY_NONE,
   .routineerrorpolicy = ROUTINE_ERROR_POLICY_NONE,
   .queuecapacity = 10
  };

  buffio::buffsocket server(serverinfo);
  server.clienthandler = clienthandler;
  
  
  buffio::instance runner;
  runner.fireeventloop(server,queuepolicy,EVENTLOOP_SYNC); 
 
  return 0;
 };
