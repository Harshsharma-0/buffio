#include <iostream>
#define BUFFIO_IMPLEMENTATION
#define BUFFIO_DEBUG_BUILD
#define BUFFIO_LOG_ERROR
#define BUFFIO_LOG_LOG
#define BUFFIO_LOG_TRACE

#include "./buffio.hpp"


buffioroutine clientcall2(){
    std::cout<<"hello client3"<<std::endl;
    throw std::runtime_error("ayse waise 2");
    
   
  ioreturn -1;
};

buffioroutine clientcall(){
    std::cout<<"hello client2"<<std::endl;
   
try{
   buffiocatch(iowait clientcall2()).throwerror();
  }catch(const std::exception &e){
    std::cout<<e.what()<<std::endl;
  };

     std::cout<<"hello client2 exit"<<std::endl;
  // throw std::runtime_error("ayse waise");

    ioreturn -1;
};

buffioroutine clienthandler(clientinfo info){

  std::cout<<"hello client : "<<info.address<<":"<<info.portnumber<<std::endl;
 
 try{
   buffiocatch(iowait clientcall()).throwerror();
  }catch(const std::exception &e){
    std::cout<<e.what()<<std::endl;
  };
  for(int i = 0; i < 100 ; i++)
     ioyeild 0;

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
