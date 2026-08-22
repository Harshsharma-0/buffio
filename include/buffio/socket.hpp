#ifndef BUFFIO_SOCKET
#define BUFFIO_SOCKET

#include "buffio/optable.hpp"
#include "buffio/defs.hpp"

#define BF_SOCKET_API inline

namespace buffio {

class socket{
  
 buffio_fd fd;
 int flags;

};

  /*
 BF_SOCKET_API BFD create(std::string const &address,uint32_t port){
  return buffio_os_socket(address,port);
 };
 */
}; // namespace buffio
#undef BF_SOCKET_API
#endif
