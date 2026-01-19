#ifndef BUFFIO_SOCK_IMPLEMENTATION
#define BUFFIO_SOCK_IMPLEMENTATION

/*
 * Error codes range reserved for buffiosock
 *  [0 - 999]
 *  [Note] Errorcodes are negative value in this range.
 *  0 <= errorcode <= 999
 *  Naming convention is scheduled to change in future to Ocaml style
 */

#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#if !defined(BUFFIO_IMPLEMENTATION)
#include "buffioenum.hpp" // header for opcode
#include "buffiolfqueue.hpp"
#include "buffiomemory.hpp"
#endif

enum class buffio_fd_family : uint32_t {
    none = 0,
    file = 1,
    pipe = 2,
//    fifo = 3,
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

enum class buffioFdActive : uint32_t {
    none = 0,
    ipv4 = 1,
    ipv6 = 2,
//    fifo = 3,
    file = 4,
    pipe = 5,
    local = 6
};

enum class buffio_fd_error : int {
    none = 0,
    socket = -101,
    bind = -102,
    open = -103,
    file_path = -104,
    socket_address = -105,
    portnumber = -106,
    pipe = -107,
    fifo = -108,
    fifo_path = -109,
    occupied = -110,
    family = -112,
    address_ipv4 = -113,
    address_ipv6 = -114,
    fcntl = -115
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
 *
 */

class buffiofd {
    // magic magic things
    union fd_union {
        struct {
            int fd;
            bool unlink_on_close;
            sockaddr_storage addr;
            socklen_t addr_len;
        } sock;
        int pipe_fds[2];
        int file_fd;
    };

public:
    buffiofd()
    {
        fd_family = buffio_fd_family::none;
        active = buffioFdActive::none;
        data = { 0 };
    };

    // function to create a ipsocket
    [[nodiscard]] buffio_fd_error createsocket(struct buffiofdsocketinfo& info)
    {
        // proceed only if there is no fd currently opened in the context
        if (active != buffioFdActive::none)
            return buffio_fd_error::occupied;

        int domain = 0;
        int type = 0;

        sockaddr_storage addr = { 0 };
        socklen_t addr_len = 0;
        bool unlink_on_close = false;

        switch (info.family) {
        case buffio_fd_family::ipv4_tcp:
        case buffio_fd_family::ipv4_udp: {
            domain = AF_INET;
            type = info.family == buffio_fd_family::ipv4_tcp ? SOCK_STREAM : SOCK_DGRAM;

            if (!info.address || info.portnumber <= 0 || info.portnumber > 65535)
                return buffio_fd_error::portnumber;

            sockaddr_in* in = reinterpret_cast<sockaddr_in*>(&addr);
            in->sin_family = AF_INET;
            in->sin_port = htons(info.portnumber);
            if (inet_pton(AF_INET, info.address, &in->sin_addr) != 1)
                return buffio_fd_error::address_ipv4;
            addr_len = sizeof(sockaddr_in);
        } break;
        case buffio_fd_family::local_udp:
        case buffio_fd_family::local_tcp: {

            if (info.address == nullptr)
                return buffio_fd_error::socket_address;
            domain = AF_UNIX;
            type = info.family == buffio_fd_family::local_tcp ? SOCK_STREAM : SOCK_DGRAM;

            sockaddr_un* un = reinterpret_cast<sockaddr_un*>(&addr);
            un->sun_family = AF_UNIX;
            addr_len = sizeof(sockaddr_un);

            size_t len = strlen(info.address);
            if (len >= sizeof(un->sun_path))
                return buffio_fd_error::socket_address;
            std::memcpy(un->sun_path, info.address, len + 1);
            unlink_on_close = true;
        } break;
        default:
            return buffio_fd_error::family;
            break;
        }
        int fd = ::socket(domain, type, 0);
        if (fd < 0)
            return buffio_fd_error::socket;
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), addr_len) != 0) {
            ::close(fd);
            return buffio_fd_error::bind;
        }
        data.sock.fd = fd;
        data.sock.addr = addr;
        data.sock.unlink_on_close = unlink_on_close;
        data.sock.addr_len = addr_len;
        fd_family = info.family;
        active = (domain == AF_INET) ? buffioFdActive::ipv4 : buffioFdActive::local;
        return buffio_fd_error::none;
    };

    // function to create a pipe
    [[nodiscard]]
    buffio_fd_error createpipe(buffio_fd_block block = buffio_fd_block::block)
    {

        if (active != buffioFdActive::none)
            return buffio_fd_error::occupied;

        if (::pipe(data.pipe_fds) != 0)
            return buffio_fd_error::pipe;
        if (block == buffio_fd_block::no_block) {
            if (fcntl(data.pipe_fds[0], F_SETFD, O_NONBLOCK) == -1) {
                ::close(data.pipe_fds[0]);
                return buffio_fd_error::fcntl;
            }
            if (fcntl(data.pipe_fds[1], F_SETFD, O_NONBLOCK) == -1) {
                ::close(data.pipe_fds[0]);
                ::close(data.pipe_fds[1]);

                return buffio_fd_error::fcntl;
            }
        }
        fd_family = buffio_fd_family::pipe;
        active = buffioFdActive::pipe;

        return buffio_fd_error::none;
    };
 
  // fifo must be managed by the user, only helper function to create fifo
    [[nodiscard]] 
       buffio_fd_error createfifo(const char* path = nullptr,mode_t mode = 0666){
     if(path == nullptr) return buffio_fd_error::fifo_path;
     if(mkfifo(path,mode) == 0) 
       return buffio_fd_error::none;
     return buffio_fd_error::fifo;
    }

    [[nodiscard]] int createfile(const char* path = nullptr)
    {
        // TODO: add file support
        if (path == nullptr)
            return -1;
        return 0;
    };

    buffiofd(const buffiofd&) = delete;
    buffiofd& operator=(const buffiofd&) = delete;

    // return void
    void shutfd() noexcept
    {
        switch (active) {
        case buffioFdActive::file: {
            // not handled yet
        } break;
        case buffioFdActive::pipe: {
            ::close(data.pipe_fds[0]);
            ::close(data.pipe_fds[1]);
        } break;
        case buffioFdActive::ipv4: {
            ::close(data.sock.fd);
        } break;
        case buffioFdActive::local: {
            if (data.sock.unlink_on_close == true) {
                sockaddr_un* un = reinterpret_cast<sockaddr_un*>(&data.sock.addr);
                ::unlink(un->sun_path);
            }
            ::close(data.sock.fd);

        } break;
        };

        fd_family = buffio_fd_family::none;
        active = buffioFdActive::none;
        data = { 0 };
    };
    ~buffiofd() { shutfd(); };

private:
    buffio_fd_family fd_family;
    buffioFdActive active;
    fd_union data;
};

#endif
