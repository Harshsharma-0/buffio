#include <iostream>
#define BUFFIO_IMPLEMENTATION
//#define BUFFIO_DEBUG_BUILD
//#define BUFFIO_LOG_ERROR
//#define BUFFIO_LOG_LOG
//#define BUFFIO_LOG_TRACE
//#define BUFFIO_LOG_WARN

#include "./buffio.hpp"


buffioroutine clientcall2(int code){
//    std::cout<<"hello client3"<<std::endl;
    for(int i = 0; i < 1000 ; i++){
  //     std::cout<<"hello client3 generating value: "<<code<<std::endl;
       buffioyeild 0;
     }
//    throw std::runtime_error("ayse waise 2");   
    buffioreturn 0;

};

buffioroutine clientcall(int code){
//   std::cout<<"i am hiae"<<std::endl;
   
try{
   buffiocatch(buffiowait clientcall2(code)).throwerror();
  }catch(const std::exception &e){
    std::cout<<e.what()<<std::endl;
  };
  
  //  std::cout<<"throuwing a exception"<<std::endl;

  // throw std::runtime_error("ayse waise");

    buffioreturn 0;
};

buffioroutine pushedtask(){
  std::cout<<"pushed task"<<std::endl;

try{
   buffiocatch(buffiowait clientcall2(0)).throwerror();
  }catch(const std::exception &e){
    std::cout<<e.what()<<std::endl;
  };

  buffioyeild 0;
  buffioreturn 0;
};

buffioroutine clienthandler(int code){

 //std::cout<<"hello client start "<<code<<std::endl;

  buffiopushtaskinfo info;
  for(int i = 0; i < 10 ; i++){
   info.task = pushedtask();
   buffiopush info;
  }


 try{
    buffiocatch(buffiowait clientcall(code)).throwerror();
  }catch(const std::exception &e){
 
    std::cout<<e.what()<<std::endl;
 }

    
   std::cout<<"hello client exit : "<<code<<std::endl;
  buffioreturn 0;
};

int main(){
  
  buffioinfo serverinfo = {
   .address = "127.0.0.1",
   .portnumber = 8081,
   .listenbacklog = 10,
   .socktype = BUFFIO_SOCK_TCP,
   .sockfamily = BUFFIO_FAMILY_IPV4
  };

  
  
  buffio::instance runner;
  runner.push(clienthandler(1));
  runner.push(clienthandler(2));
  runner.push(clienthandler(3));
  runner.push(clienthandler(4));
  runner.push(clienthandler(5));
  runner.fireeventloop(EVENTLOOP_SYNC);


   return 0;
 };
