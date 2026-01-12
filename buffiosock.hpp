#ifndef __BUFFIO_SOCK_IMPLEMENTATION__
#define __BUFFIO_SOCK_IMPLEMENTATION__

/*
* Error codes range reserved for buffiosock
*  [0 - 999]
*  0 <= errorcode <= 999
*/

#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <errno.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>

#define BUFFIO_FAMILY_LOCAL 1 //AF_UNIX;
#define BUFFIO_FAMILY_IPV4  2 //AF_INET;
#define BUFFIO_FAMILY_IPV6  3 //AF_INET6;
#define BUFFIO_FAMILY_FILE  4 //0;
#define BUFFIO_FAMILY_PIPE  5 //1;
#define BUFFIO_FAMILY_FIFO  6 //3;

#define BUFFIO_SOCK_TCP   1     // SOCK_STREAM;
#define BUFFIO_SOCK_UDP   2    //SOCK_DGRAM;
#define BUFFIO_SOCK_RAW   3    //SOCK_RAW;
#define BUFFIO_SOCK_ASYNC 4   // SOCK_NONBLOCK;

#define bf_so_address_ok 1
#define bf_so_portnumber_ok (1 << 1)
#define bf_so_listenbacklog_ok (1 << 2)
#define bf_so_socktype_ok (1 << 3)
#define bf_so_sockfamily_ok (1 << 4)
#define bf_so_ok 0xF0;

#define bf_so_address_err 1
#define bf_so_portnumber_err 2
#define bf_so_listenbacklog_err 3
#define bf_so_socktype_err 4
#define bf_so_sockfamily_err 5


//32-bit number means 8 solts

// 0-15 portnumber unsigned 16-bit 
// 16-23 listenbacklog unsigned 8-bits for listen backlog
// 24-27 socktype
// 28-31 sockfamily

struct buffiosockinfo{
  const char *address;
  int mask;
  uint32_t infocomined;
};

struct buffiosockinfostr{
 int mask;
 const char *protocol;
};

#define bf_ci_address_ok 1
#define bf_ci_clientfd_ok (1 << 1)
#define bf_ci_portnumber_ok (1 << 2)
#define bf_ci_ok (bf_ci_address | bf_ci_clientfd_ok | bf_ci_clientfd_ok)

#define bf_ci_address_err 100
#define bf_ci_clientfd_err 101
#define bf_ci_portnumber_err 102

struct clientinfo {
  const char *address;
  int clientfd;
  int portnumber;
 };

class buffiosocketview{
  buffiosocketview():data(nullptr),mask(0),sock(-1){}
  buffiosocketview(int sock):data(nullptr),mask(0),sock(-1){}
private:
  int sock;
  int mask;
  void *data;
};


class buffiosocket{

  union __sockinfointernal{
   struct buffiosockinfo rawinfo;
   struct buffiosockinfo strinfo;
  }
 public:
 
  buffiosocket(buffiosockinfo &ioinfo) : linfo(ioinfo), socketfd(-1) , sockfdblocking(false){};
  buffiosocket():socketfd(-1),socketfdblocking(false){}
  

 ~buffiosocket(){ 
 };

private:

};

#endif

/*
*
*   int createipv4socket(buffiosockinfo &ioinfo , int *socketfd){

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

   int createlocalsocket(buffiosockinfo &ioinfo ,int *socketfd){
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

* */
