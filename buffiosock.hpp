#ifndef __BUFFIO_SOCK_IMPLEMENTATION__
#define __BUFFIO_SOCK_IMPLEMENTATION__

#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <errno.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>

constexpr int BUFFIO_FAMILY_LOCAL = AF_UNIX;
constexpr int BUFFIO_FAMILY_IPV4 = AF_INET;
constexpr int BUFFIO_FAMILY_IPV6 = AF_INET6;
constexpr int BUFFIO_FAMILY_CAN = AF_CAN;
constexpr int BUFFIO_FAMILY_NETLINK = AF_NETLINK;
constexpr int BUFFIO_FAMILY_LLC = AF_LLC;
constexpr int BUFFIO_FAMILY_BLUETOOTH = AF_BLUETOOTH;

constexpr int BUFFIO_SOCK_TCP = SOCK_STREAM;
constexpr int BUFFIO_SOCK_UDP = SOCK_DGRAM;
constexpr int BUFFIO_SOCK_RAW = SOCK_RAW;
constexpr int BUFFIO_SOCK_ASYNC = SOCK_NONBLOCK;

struct buffioinfo {
  const char *address;
  int portnumber;
  int listenbacklog;
  int socktype;
  int sockfamily;
};

struct clientinfo {
  const char *address;
  int clientfd;
  int portnumber;
 };

struct buffiobuffer{
  char *data;
  size_t filled;
  size_t size;
  void *next;
};

class buffiosocket{

 public:
 
  buffiosocket(buffioinfo &ioinfo) : linfo(ioinfo), socketfd(-1) , sockfdblocking(true){

    if(!(ioinfo.socktype & BUFFIO_SOCK_ASYNC)) sockfdblocking = true;

     switch(ioinfo.sockfamily){
      case BUFFIO_FAMILY_LOCAL: if(createlocalsocket(ioinfo,&socketfd) < 0) return;
      break;
      case BUFFIO_FAMILY_IPV4:  if(createipv4socket(ioinfo,&socketfd) < 0) return;
      break;
      case BUFFIO_FAMILY_IPV6:
      break;
      case BUFFIO_FAMILY_BLUETOOTH:
      break;
      default: BUFFIO_ERROR(" UNKNOWN TYPE SOCKET CREATION FAILED "); return;
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
                  BUFFIO_ERROR(" SOCKET BINDING FAILED : ", strerror(errno));
                  BUFFIO_LOG(" PORT : ", ioinfo.portnumber," IP ADDRESS : ", ioinfo.address);
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
                  BUFFIO_ERROR("SOCKET BINDING FAILED : ",strerror(errno));
                  close(*socketfd);
                  *socketfd = -1;
          return -1;
         }
         aacceptedaddress = (char *)ioinfo.address; 
    return 0;
   };

 ~buffiosocket(){ 
    if(socketfd > 0){
      if(close(socketfd) < 0){
        BUFFIO_ERROR(" SOCKET CLOSE FAILURE");
     };
     if(linfo.sockfamily  == BUFFIO_FAMILY_LOCAL) unlink(linfo.address);

     BUFFIO_LOG(" SOCKET CLOSED SUCCESFULLY");
    }
 };

 int listensock(){
    linfo.listenbacklog = linfo.listenbacklog < 0 ? 0 : linfo.listenbacklog;
    if(listen(socketfd,linfo.listenbacklog) < 0){
        BUFFIO_ERROR(" FAILED TO LISTEN ON SOCKET \n reason : ",strerror(errno));
        return -1;
    }
        BUFFIO_INFO(" listening on socket : \n" 
                        "              address - ",linfo.address,
                        "\n              port - ",linfo.portnumber);

    return 0;
  };

acceptreturn acceptsock(buffioroutine (*clienthandler)(clientinfo cinfo)){

      switch(linfo.sockfamily){
        case BUFFIO_FAMILY_LOCAL:  afd = accept(socketfd,NULL,NULL); break;
        case BUFFIO_FAMILY_IPV4: { 
           sockinfolen = sizeof(struct sockaddr_in); 
           struct sockaddr_in address_in;
           afd = accept(socketfd,(struct sockaddr*)&address_in,&sockinfolen);
           if(afd > 0){
             portnumber = ntohs(address_in.sin_port);
             aacceptedaddress  = inet_ntoa((struct in_addr)address_in.sin_addr); 
           }
         }
        break;
        case BUFFIO_FAMILY_IPV6:  break;
     }; 

    if(afd < 0){
       if(errno == EAGAIN || errno == EWOULDBLOCK)
             return {.errorcode = BUFFIO_ACCEPT_STATUS_NA , .handle = NULL};
      
       BUFFIO_ERROR(" Failed to accept connection, \n reason: ",strerror(errno),errno);
             return {.errorcode = BUFFIO_ACCEPT_STATUS_ERROR , .handle = NULL};
    }
      
      cinfo = {.address = aacceptedaddress , .clientfd = afd,.portnumber = portnumber};
      BUFFIO_LOG(" Accepted a connection : address -> ",aacceptedaddress , 
                 "\n                           port -> ",portnumber);

      if(!clienthandler) return {.errorcode = BUFFFIO_ACCEPT_STATUS_NO_HANDLER,.handle = NULL};  
      return {.errorcode = BUFFIO_ACCEPT_STATUS_SUCCESS , .handle = clienthandler(cinfo)};
};

  // TODO: DO ERROR CHECK IN FUNCTIONS:
  void setfdblocking(){ 
    if(!sockfdblocking){
      int flags = fcntl(socketfd,F_GETFL,0);
      flags &= ~O_NONBLOCK;
      fcntl(socketfd,F_SETFL,flags);
      sockfdblocking = true;
    }
  };
  void setfdnonblocking(){
    if(sockfdblocking){
      int flags = fcntl(socketfd,F_GETFL,0);
      flags |= O_NONBLOCK;
      fcntl(socketfd,F_SETFL,flags);
      sockfdblocking = false;
    }
  };

 int socketfd;
 bool sockfdblocking;

 buffioinfo linfo;
 clientinfo cinfo;

 private:
      socklen_t sockinfolen = 0;
      char *aacceptedaddress = nullptr;
      int afd = -1;
      int portnumber = 0;
};


#endif
