#ifndef __BUFFIO_SOCK_IMPLEMENTATION__
#define __BUFFIO_SOCK_IMPLEMENTATION__

/*
 * Error codes range reserved for buffiosock
 *  [0 - 999]
 *  [Note] Errorcodes are negative value in this range.
 *  0 <= errorcode <= 999
 */

#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <regex>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define ipv4_regex "^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])$"

constexpr const char* buffio_ipv_localhost = "localhost";
constexpr const char *buffio_fd_default_address = "127.0.0.1";

constexpr const int buffio_fd_default_port = 8081;

enum class buffio_fd_family : uint32_t {
    none = 0,
    file = 1,
    pipe = 2,
    fifo = 3,
    ipv4_tcp = 4,
    ipv4_udp = 5,
    ipv6_tcp = 6,
    ipv6_udp = 7,
    ip_raw = 8,
    local_tcp = 9,
    local_udp = 10
};

enum class buffio_fd_block : uint32_t {
    block = 0,
    no_block = 1,
};

enum class buffio_fd_error: int {
 none           = 0,
 socket         = -1,
 bind           = -2,
 open           = -3,
 file_path      = -4,
 socket_address = -5,
 port_number    = -6,
 pipe           = -7,
 fifo           = -8,
 fifo_path      = -9,
 occupied       = -10,
 family         = -11,
 address_ipv4   = -12,
 address_ipv6   = -13,
 portnumber     = -14,
};


// used for ip socket
struct buffiofdsocketinfo {
    const char* address;
    int portnumber;
    buffio_fd_family family;
    buffio_fd_block block;
};

struct buffiofdinfo {
    const char* address;
    buffio_fd_family family;
    buffio_fd_block block;
};

struct buffiofdreq_rw {
    buffio_fd_opcode opcode;
    char* buffer;
    size_t len;
    void* data;
};

struct buffiofdreq_add {
    buffio_fd_opcode opcode;
    int fd;
    void* data;
};

struct buffiofdreq_paged {
    struct buffiofdreq_rw data; // opcode will come from the data field for this;
    buffiopage* page;
};

union buffiofdreq {
    struct buffiofdreq_rw rw;
    struct buffiofdreq_paged paged;
    struct buffiofdreq_add add;
};

// buffiosockbroker support both pages write and read and raw buffer read write
// use pages to read large amount of data
/*
 *

 */


class buffiofd {


// magic magic things
union __socketunified{
      struct{
         int fd;
         int laddr;
         union{
           struct sockaddr_in ipv4;
           struct sockaddr_in6 ipv6;
           struct sockaddr_un local;
         }info;
      }sock;
      int pipe_fds[2];
      struct file_info{
         char *address;
         int fd;
    } file_info;
};


public:
    buffiofd(){
        fd_family = buffio_fd_family::none;
        memset(&buffio_fd_info, '\0', sizeof(union __socketunified));
    };

    // function to create a ipsocket
    [[nodiscard]] buffio_fd_error createsocket(struct buffiofdsocketinfo& info){
        // proceed only if there is no fd currently opened in the context
        if (fd_family == buffio_fd_family::none ) {

            union __socketunified sock_info = { 0 };
            std::regex ipvalidator(ipv4_regex);

            int socketfamily = AF_INET;
            int sockettype = SOCK_STREAM;
            size_t field_size = 0;
     
            memset(&sock_info, '\0', sizeof(union __socketunified));

            switch (info.family) {
            case buffio_fd_family::local_udp:
                socketfamily = AF_UNIX;
                sockettype = SOCK_DGRAM;
                [[fallthrough]];

            case buffio_fd_family::local_tcp: {
                socketfamily = AF_UNIX;
                sock_info.sock.info.local.sun_family = socketfamily;
                field_size = sizeof(struct sockaddr_un);

                if (info.address == nullptr) break;
                int len = strlen(info.address);
                len -= 1;
                if(len > sizeof(sockaddr_un))
                    return buffio_fd_error::socket_address;

                //TODO: check if the path is valid string of path 
                strncpy(sock_info.sock.info.local.sun_path, info.address,
                         sizeof(sock_info.sock.info.local.sun_path) - 1);
                 sock_info.sock.laddr = 1;
                 fd_family =  info.family;

            } break;
            case buffio_fd_family::ipv4_udp:
                sockettype = SOCK_DGRAM;
                [[fallthrough]];

            case buffio_fd_family::ipv4_tcp: {

                if(info.portnumber <= 0)  
                      return buffio_fd_error::portnumber;
                if(info.address == nullptr)
                      return buffio_fd_error::address_ipv4;
                if(strcmp(info.address, buffio_ipv_localhost) == 0)
                      break;
                if (!std::regex_match(info.address,ipvalidator))
                      return buffio_fd_error::address_ipv4;

                field_size = sizeof(struct sockaddr_in);
                sock_info.sock.info.ipv4.sin_family = socketfamily;
                sock_info.sock.info.ipv4.sin_port = htons(info.portnumber);
                sock_info.sock.info.ipv4.sin_addr.s_addr =inet_addr(info.address);
                fd_family =  info.family;


                }break;
          /* TODO: implement ipv6
            case buffio_fd_family::ipv6_udp: {
                socketfamily = AF_INET6;
                sockettype = SOCK_DGRAM;
            } break;

            case buffio_fd_family::ipv6_tcp: {
                socketfamily = AF_INET6;
            } break;
            */
            default:
                return buffio_fd_error::family;
                break;
            }

            int sockfd = socket(socketfamily, sockettype, 0);
            if(sockfd < 0){
              fd_family = buffio_fd_family::none;      
              return buffio_fd_error::socket;
            }
            sock_info.sock.fd = sockfd;
            buffio_fd_info = sock_info;
            // 0 if local socket don't want to bind to addres
            if (field_size == 0) 
              return buffio_fd_error::none;

            if(bind(sockfd,(struct sockaddr*)&sock_info.sock.info,field_size) < 0)
            {
                close(sockfd);
                fd_family = buffio_fd_family::none;
                buffio_fd_info = {0};
                return buffio_fd_error::bind;
            } 
            return buffio_fd_error::none;
        };
        return buffio_fd_error::occupied;
    };

    // function to create a pipe
    [[nodiscard]] 
     buffio_fd_error createpipe(buffio_fd_block block = buffio_fd_block::block)
    { 
       if(fd_family != buffio_fd_family::none)
                    return buffio_fd_error::occupied;
        int pipefd = pipe(buffio_fd_info.pipe_fds); 
        if(pipefd < 0) return buffio_fd_error::pipe;
        fd_family = buffio_fd_family::pipe;
        return buffio_fd_error::none;
    };

    // function to create a fifo
    [[nodiscard]]
    buffio_fd_error createfifo(const char *path = nullptr){
     if(path == nullptr) return buffio_fd_error::fifo_path;
     if(fd_family != buffio_fd_family::none)
                    return buffio_fd_error::occupied;

     int fifofd = mkfifo(path,0);
     if(fifofd < 0) return buffio_fd_error::fifo; 
     buffio_fd_info.file_info.fd = fifofd;
     //TODO: make path also available in the field; 
     fd_family = buffio_fd_family::fifo;

        return buffio_fd_error::none;
    };

    // function to open a file
    [[nodiscard]] int createfile(const char *path = nullptr)
    {
     // TODO: add file support
        if(path == nullptr) return -1;
        return 0;
    };

    buffiofd(const buffiofd&) = delete;
    buffiofd& operator=(const buffiofd&) = delete;

    // return void 
    void shutfd(){
      if(fd_family == buffio_fd_family::none)
                  return;

      /* sock is the majority, so it is intilised by default to close,
       * if there any other case the fd is changed and that is closed
      */
      int fd = buffio_fd_info.sock.fd; 
      switch(fd_family){
        case buffio_fd_family::file : [[fallthrough]]; // don't remove
        case buffio_fd_family::fifo : {
         fd = buffio_fd_info.file_info.fd;
         } break;
        case buffio_fd_family::pipe : {
         close(buffio_fd_info.pipe_fds[0]); 
         close(buffio_fd_info.pipe_fds[1]); 
         return;
        }break;
      /*
        case buffio_fd_family::ipv4_tcp : {} break;
        case buffio_fd_family::ipv4_udp : {} break;
        case buffio_fd_family::ipv6_tcp : {} break;
        case buffio_fd_family::ipv6_udp : {} break;
        case buffio_fd_family::ip_raw   : {} break;
      */
        case buffio_fd_family::local_tcp:
         [[fallthrough]];
        case buffio_fd_family::local_udp:{
         if(buffio_fd_info.sock.laddr == 1)
            unlink(buffio_fd_info.sock.info.local.sun_path);
         } break;
      }
      close(fd);

    };
    ~buffiofd(){shutfd();};

private:
    buffio_fd_family fd_family;
    union __socketunified buffio_fd_info; 
};

#endif
