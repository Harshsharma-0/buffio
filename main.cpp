#include <iostream>
#define BUFFIO_IMPLEMENTATION
#define BUFFIO_DEBUG_BUILD
#define BUFFIO_LOG_ERROR
#define BUFFIO_LOG_LOG
#define BUFFIO_LOG_TRACE

#include "./buffio.hpp"


buffioroutine clientcall(){
    std::cout<<"hello client2"<<std::endl;
    //throw std::runtime_error("ayse waise");
    ioreturn -1;
};

buffioroutine clienthandler(clientinfo info){

  std::cout<<"hello client : "<<info.address<<":"<<info.portnumber<<std::endl;
  buffiocatch(iowait clientcall()) = [](const std::exception &e,int successcode){
    std::cout<<e.what()<<successcode<<std::endl;
  };

  std::cout<<"hello client exit"<<std::endl;
  ioreturn 0;
};

int main(){
  
  buffioinfo serverinfo = {
   .address = "127.0.0.1",
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
