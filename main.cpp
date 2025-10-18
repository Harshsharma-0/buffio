#include <iostream>
#define BUFFIO_IMPLEMENTATION
#define BUFFIO_DEBUG
#include "./buffio.hpp"


int main(){
  /*
  buffioinfo serverinfo = {
   .address = "127.0.0.1",
   .portnumber = 8080,
   .maxclient = 500,
   .socktype = BUFFIO_SOCK_TCP,
   .sockfamily = BUFFIO_FAMILY_IPV4
  };

  buffio::buffsocket server(serverinfo);
  */

  buffio::instance runner;
  runner.fireeventloop(); 
  return 0;
 };
