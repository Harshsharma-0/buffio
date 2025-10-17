#ifndef BUFF_IO
#define BUFF_IO

#if defined(BUFFIO_DEBUG)
  #include "./buffiolog.hpp"
#endif

#if defined(BUFFIO_IMPLEMENTATION)

#define MAX_PATH_NAME 50

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include <cstring>

typedef enum BUFFIO_FAMILY_TYPE{
 BUFFIO_FAMILY_LOCAL = AF_UNIX,
 BUFFIO_FAMILY_IPV4 = AF_INET,
 BUFFIO_FAMILY_IPV6 = AF_INET6,
 BUFFIO_FAMILY_CAN  = AF_CAN,
 BUFFIO_FAMILY_NETLINK = AF_NETLINK,
 BUFFIO_FAMILY_LLC = AF_LLC,
 BUFFIO_FAMILY_BLUETOOTH = AF_BLUETOOTH,
}BUFFIO_FAMILY_TYPE;

typedef enum BUFFIO_SOCK_TYPE{
 BUFFIO_SOCK_TCP = SOCK_STREAM,
 BUFFIO_SOCK_UDP = SOCK_DGRAM,
 BUFFIO_SOCK_RAW = SOCK_RAW,
}BUFFIO_SOCK_TYPE;


typedef struct buffioinfo{
 const char *address;
 int portnumber;
 int maxclient;
 BUFFIO_SOCK_TYPE socktype;
 BUFFIO_FAMILY_TYPE sockfamily;
}buffioinfo;

namespace buffio{

class buffsocket{

 public:
 
  buffsocket(buffioinfo &ioinfo) : linfo(ioinfo){
     switch(ioinfo.sockfamily){
      case BUFFIO_FAMILY_LOCAL:
        if(createlocalsocket(ioinfo,&socketfd) < 0)
           return;
      break;
      case BUFFIO_FAMILY_IPV4:
        if(createipv4socket(ioinfo,&socketfd) < 0)
            return;
      break;
      case BUFFIO_FAMILY_IPV6:
      break;
      case BUFFIO_FAMILY_CAN:
      break;
      case BUFFIO_FAMILY_NETLINK:
      break;
      case BUFFIO_FAMILY_LLC:
      break;
      case BUFFIO_FAMILY_BLUETOOTH:
      break;
      default:
       BUFFIO_LOG(ERROR,"UNKNOWN TYPE SOCKET CREATION FAILED");
      return;
      break;
    }; 
     return;
  };
   int createipv4socket(buffioinfo &ioinfo , int *socketfd){

      *socketfd = socket(ioinfo.sockfamily,ioinfo.socktype,0);
       struct sockaddr_in addr;
       memset(&addr,'\0',sizeof(sockaddr_in));
       addr.sin_family = ioinfo.sockfamily;
       addr.sin_port = htons(ioinfo.portnumber);
       addr.sin_addr.s_addr = inet_addr(ioinfo.address);
       if(bind(*socketfd, (struct sockaddr *)&addr,sizeof(struct sockaddr_in)) < 0){
                  BUFFIO_LOG(ERROR," SOCKET BINDING FAILED : ", strerror(errno));
                  BUFFIO_LOG(LOG," PORT : ", ioinfo.portnumber," IP ADDRESS : ", ioinfo.address);

                  close(*socketfd);
                  *socketfd = -1;
          return -1;
       }

    return 0;
   }
   int createlocalsocket(buffioinfo &ioinfo ,int *socketfd){
        *socketfd = socket(ioinfo.sockfamily,ioinfo.socktype,0);
         struct sockaddr_un addr;
         memset(&addr,'\0',sizeof(sockaddr_un));
         addr.sun_family = BUFFIO_FAMILY_LOCAL;
         strncpy(addr.sun_path,ioinfo.address,sizeof(addr.sun_path) - 1);
         if(bind(*socketfd, (struct sockaddr *)&addr,sizeof(struct sockaddr_un)) < 0){
                  BUFFIO_LOG(ERROR,"SOCKET BINDING FAILED : ",strerror(errno));
                  close(*socketfd);
                  *socketfd = -1;
          return -1;
         }
          
    return 0;
   };

 ~buffsocket(){ 
    if(socketfd > 0){
      if(close(socketfd) < 0){
        BUFFIO_LOG(ERROR,"SOCKET CLOSE FAILURE");
     };
     unlink(linfo.address);
     BUFFIO_LOG(LOG,"SOCKET CLOSED SUCCESFULLY");
    }
 };

 int socketfd = -1;
 buffioinfo linfo;
};

};
#endif
#endif
