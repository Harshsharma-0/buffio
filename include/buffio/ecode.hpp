//[NOTE]: Header Generated via help of chatgpt


#pragma once

#include "buffio/config.hpp"

#include <cerrno>

#if defined(BUFFIO_OS_WINDOWS)
    // winsock2.h must come before windows.h.
    #include <winsock2.h>
    #include <windows.h>
    #include <winerror.h>
#endif

namespace buffio {

// ============================================================================
// Canonical BuffIO error definitions
// ============================================================================
//
// BuffIO owns these numbers.
//
// Rules:
//   - Values are always negative when exposed as error codes.
//   - Numbers are stable across platforms.
//   - Native OS errno/error values must NEVER determine BuffIO's ABI.
//   - The numeric values intentionally follow familiar POSIX errno values
//     where practical.
//
// X(name, number, description)
// ============================================================================

#define BUFFIO_ERROR_TABLE(X)                                             \
    X(B_EPERM,            1,   "Operation not permitted")                  \
    X(B_ENOENT,           2,   "No such file or directory")                \
    X(B_ESRCH,            3,   "No such process")                          \
    X(B_EINTR,            4,   "Interrupted system call")                   \
    X(B_EIO,              5,   "Input/output error")                       \
    X(B_ENXIO,            6,   "No such device or address")                 \
    X(B_E2BIG,            7,   "Argument list too long")                   \
    X(B_ENOEXEC,          8,   "Exec format error")                        \
    X(B_EBADF,            9,   "Bad file descriptor")                      \
    X(B_ECHILD,          10,   "No child processes")                       \
    X(B_EAGAIN,          11,   "Resource temporarily unavailable")         \
    X(B_ENOMEM,          12,   "Cannot allocate memory")                   \
    X(B_EACCES,          13,   "Permission denied")                        \
    X(B_EFAULT,          14,   "Bad address")                              \
    X(B_EBUSY,           16,   "Device or resource busy")                  \
    X(B_EEXIST,          17,   "File exists")                              \
    X(B_EXDEV,           18,   "Invalid cross-device link")                \
    X(B_ENODEV,          19,   "No such device")                           \
    X(B_ENOTDIR,         20,   "Not a directory")                         \
    X(B_EISDIR,          21,   "Is a directory")                           \
    X(B_EINVAL,          22,   "Invalid argument")                         \
    X(B_ENFILE,          23,   "Too many open files in system")            \
    X(B_EMFILE,          24,   "Too many open files")                      \
    X(B_ENOTTY,          25,   "Inappropriate ioctl for device")           \
    X(B_EFBIG,           27,   "File too large")                           \
    X(B_ENOSPC,          28,   "No space left on device")                  \
    X(B_ESPIPE,          29,   "Illegal seek")                             \
    X(B_EROFS,           30,   "Read-only file system")                    \
    X(B_EMLINK,          31,   "Too many links")                           \
    X(B_EPIPE,           32,   "Broken pipe")                             \
    X(B_EDOM,            33,   "Numerical argument out of domain")         \
    X(B_ERANGE,          34,   "Numerical result out of range")            \
    X(B_EDEADLK,         35,   "Resource deadlock avoided")                \
    X(B_ENAMETOOLONG,    36,   "File name too long")                       \
    X(B_ENOLCK,          37,   "No locks available")                       \
    X(B_ENOSYS,          38,   "Function not implemented")                 \
    X(B_ENOTEMPTY,       39,   "Directory not empty")                      \
    X(B_ELOOP,           40,   "Too many levels of symbolic links")        \
    X(B_ENOMSG,          42,   "No message of desired type")               \
    X(B_EIDRM,           43,   "Identifier removed")                       \
    X(B_ENOTSOCK,        88,   "Socket operation on non-socket")           \
    X(B_EDESTADDRREQ,    89,   "Destination address required")             \
    X(B_EMSGSIZE,        90,   "Message too long")                         \
    X(B_EPROTOTYPE,      91,   "Protocol wrong type for socket")           \
    X(B_ENOPROTOOPT,     92,   "Protocol not available")                   \
    X(B_EPROTONOSUPPORT, 93,   "Protocol not supported")                   \
    X(B_EOPNOTSUPP,      95,   "Operation not supported")                  \
    X(B_EAFNOSUPPORT,    97,   "Address family not supported")             \
    X(B_EADDRINUSE,      98,   "Address already in use")                   \
    X(B_EADDRNOTAVAIL,   99,   "Cannot assign requested address")          \
    X(B_ENETDOWN,       100,   "Network is down")                          \
    X(B_ENETUNREACH,    101,   "Network is unreachable")                  \
    X(B_ENETRESET,      102,   "Network dropped connection")               \
    X(B_ECONNABORTED,   103,   "Software caused connection abort")         \
    X(B_ECONNRESET,     104,   "Connection reset by peer")                 \
    X(B_ENOBUFS,        105,   "No buffer space available")                \
    X(B_EISCONN,        106,   "Transport endpoint is already connected")  \
    X(B_ENOTCONN,       107,   "Transport endpoint is not connected")      \
    X(B_ETIMEDOUT,      110,   "Connection timed out")                     \
    X(B_ECONNREFUSED,   111,   "Connection refused")                       \
    X(B_EHOSTUNREACH,   113,   "No route to host")                         \
    X(B_EALREADY,       114,   "Operation already in progress")            \
    X(B_EINPROGRESS,    115,   "Operation in progress")                   \
    X(B_EMEDIUMTYPE,    124,   "Wrong medium type")


// ============================================================================
// Generate BuffIO constants
// ============================================================================

#define BUFFIO_DEFINE_ERROR(name, number, description) \
    inline constexpr int name = -(number);

BUFFIO_ERROR_TABLE(BUFFIO_DEFINE_ERROR)

#undef BUFFIO_DEFINE_ERROR


// Unknown/unmapped BuffIO error.
//
// This is deliberately outside the POSIX errno namespace.
inline constexpr int B_EUNKNOWN = -0x7fff;


// ============================================================================
// Error string
// ============================================================================

constexpr const char* strerror(int error) noexcept
{
    switch (error) {

#define BUFFIO_ERROR_STRING(name, number, description) \
        case name: return description;

        BUFFIO_ERROR_TABLE(BUFFIO_ERROR_STRING)

#undef BUFFIO_ERROR_STRING

        default:
            return "Unknown error";
    }
}


// ============================================================================
// Linux errno -> BuffIO
// ============================================================================
//
// This table is compiled ONLY on Linux.
//
// Consequently, Linux-specific errno values such as EMEDIUMTYPE never need
// to exist on Windows or other platforms.
// ============================================================================

#if defined(BUFFIO_OS_LINUX)

#define BUFFIO_LINUX_ERROR_TABLE(X)                                      \
    /* B_EPERM */                                                        \
    X(EPERM,          B_EPERM)                                          \
                                                                         \
    /* B_ENOENT */                                                       \
    X(ENOENT,         B_ENOENT)                                         \
                                                                         \
    /* B_ESRCH */                                                        \
    X(ESRCH,          B_ESRCH)                                          \
                                                                         \
    /* B_EINTR */                                                        \
    X(EINTR,          B_EINTR)                                          \
                                                                         \
    /* B_EIO */                                                          \
    X(EIO,            B_EIO)                                            \
                                                                         \
    /* B_ENXIO */                                                        \
    X(ENXIO,          B_ENXIO)                                          \
                                                                         \
    /* B_E2BIG */                                                        \
    X(E2BIG,          B_E2BIG)                                          \
                                                                         \
    /* B_ENOEXEC */                                                      \
    X(ENOEXEC,        B_ENOEXEC)                                        \
                                                                         \
    /* B_EBADF */                                                        \
    X(EBADF,          B_EBADF)                                          \
                                                                         \
    /* B_ECHILD */                                                       \
    X(ECHILD,         B_ECHILD)                                         \
                                                                         \
    /* B_EAGAIN */                                                       \
    X(EAGAIN,         B_EAGAIN)                                         \
                                                                         \
    /* B_ENOMEM */                                                       \
    X(ENOMEM,         B_ENOMEM)                                         \
                                                                         \
    /* B_EACCES */                                                       \
    X(EACCES,         B_EACCES)                                         \
                                                                         \
    /* B_EFAULT */                                                       \
    X(EFAULT,         B_EFAULT)                                         \
                                                                         \
    /* B_EBUSY */                                                        \
    X(EBUSY,          B_EBUSY)                                          \
                                                                         \
    /* B_EEXIST */                                                       \
    X(EEXIST,         B_EEXIST)                                         \
                                                                         \
    /* B_EXDEV */                                                        \
    X(EXDEV,          B_EXDEV)                                          \
                                                                         \
    /* B_ENODEV */                                                       \
    X(ENODEV,         B_ENODEV)                                         \
                                                                         \
    /* B_ENOTDIR */                                                      \
    X(ENOTDIR,        B_ENOTDIR)                                        \
                                                                         \
    /* B_EISDIR */                                                       \
    X(EISDIR,         B_EISDIR)                                         \
                                                                         \
    /* B_EINVAL */                                                       \
    X(EINVAL,         B_EINVAL)                                         \
                                                                         \
    /* B_ENFILE */                                                       \
    X(ENFILE,         B_ENFILE)                                         \
                                                                         \
    /* B_EMFILE */                                                       \
    X(EMFILE,         B_EMFILE)                                         \
                                                                         \
    /* B_ENOTTY */                                                       \
    X(ENOTTY,         B_ENOTTY)                                         \
                                                                         \
    /* B_EFBIG */                                                        \
    X(EFBIG,          B_EFBIG)                                          \
                                                                         \
    /* B_ENOSPC */                                                       \
    X(ENOSPC,         B_ENOSPC)                                         \
                                                                         \
    /* B_ESPIPE */                                                       \
    X(ESPIPE,         B_ESPIPE)                                         \
                                                                         \
    /* B_EROFS */                                                        \
    X(EROFS,          B_EROFS)                                          \
                                                                         \
    /* B_EMLINK */                                                       \
    X(EMLINK,         B_EMLINK)                                         \
                                                                         \
    /* B_EPIPE */                                                        \
    X(EPIPE,          B_EPIPE)                                          \
                                                                         \
    /* B_EDOM */                                                         \
    X(EDOM,           B_EDOM)                                           \
                                                                         \
    /* B_ERANGE */                                                       \
    X(ERANGE,         B_ERANGE)                                         \
                                                                         \
    /* B_EDEADLK */                                                      \
    X(EDEADLK,        B_EDEADLK)                                        \
                                                                         \
    /* B_ENAMETOOLONG */                                                 \
    X(ENAMETOOLONG,   B_ENAMETOOLONG)                                   \
                                                                         \
    /* B_ENOLCK */                                                       \
    X(ENOLCK,         B_ENOLCK)                                         \
                                                                         \
    /* B_ENOSYS */                                                       \
    X(ENOSYS,         B_ENOSYS)                                         \
                                                                         \
    /* B_ENOTEMPTY */                                                    \
    X(ENOTEMPTY,      B_ENOTEMPTY)                                      \
                                                                         \
    /* B_ELOOP */                                                        \
    X(ELOOP,          B_ELOOP)                                          \
                                                                         \
    /* B_ENOMSG */                                                       \
    X(ENOMSG,         B_ENOMSG)                                         \
                                                                         \
    /* B_EIDRM */                                                        \
    X(EIDRM,          B_EIDRM)                                           \
                                                                         \
    /* B_ENOTSOCK */                                                     \
    X(ENOTSOCK,       B_ENOTSOCK)                                       \
                                                                         \
    /* B_EDESTADDRREQ */                                                 \
    X(EDESTADDRREQ,   B_EDESTADDRREQ)                                   \
                                                                         \
    /* B_EMSGSIZE */                                                     \
    X(EMSGSIZE,       B_EMSGSIZE)                                       \
                                                                         \
    /* B_EPROTOTYPE */                                                   \
    X(EPROTOTYPE,     B_EPROTOTYPE)                                     \
                                                                         \
    /* B_ENOPROTOOPT */                                                  \
    X(ENOPROTOOPT,    B_ENOPROTOOPT)                                    \
                                                                         \
    /* B_EPROTONOSUPPORT */                                              \
    X(EPROTONOSUPPORT, B_EPROTONOSUPPORT)                               \
                                                                         \
    /* B_EOPNOTSUPP */                                                   \
    X(EOPNOTSUPP,     B_EOPNOTSUPP)                                     \
                                                                         \
    /* B_EAFNOSUPPORT */                                                 \
    X(EAFNOSUPPORT,   B_EAFNOSUPPORT)                                   \
                                                                         \
    /* B_EADDRINUSE */                                                   \
    X(EADDRINUSE,     B_EADDRINUSE)                                     \
                                                                         \
    /* B_EADDRNOTAVAIL */                                                \
    X(EADDRNOTAVAIL,  B_EADDRNOTAVAIL)                                  \
                                                                         \
    /* B_ENETDOWN */                                                      \
    X(ENETDOWN,       B_ENETDOWN)                                       \
                                                                         \
    /* B_ENETUNREACH */                                                   \
    X(ENETUNREACH,    B_ENETUNREACH)                                    \
                                                                         \
    /* B_ENETRESET */                                                     \
    X(ENETRESET,      B_ENETRESET)                                      \
                                                                         \
    /* B_ECONNABORTED */                                                  \
    X(ECONNABORTED,   B_ECONNABORTED)                                   \
                                                                         \
    /* B_ECONNRESET */                                                    \
    X(ECONNRESET,     B_ECONNRESET)                                     \
                                                                         \
    /* B_ENOBUFS */                                                       \
    X(ENOBUFS,        B_ENOBUFS)                                        \
                                                                         \
    /* B_EISCONN */                                                       \
    X(EISCONN,        B_EISCONN)                                        \
                                                                         \
    /* B_ENOTCONN */                                                      \
    X(ENOTCONN,       B_ENOTCONN)                                       \
                                                                         \
    /* B_ETIMEDOUT */                                                     \
    X(ETIMEDOUT,      B_ETIMEDOUT)                                      \
                                                                         \
    /* B_ECONNREFUSED */                                                  \
    X(ECONNREFUSED,   B_ECONNREFUSED)                                   \
                                                                         \
    /* B_EHOSTUNREACH */                                                  \
    X(EHOSTUNREACH,   B_EHOSTUNREACH)                                   \
                                                                         \
    /* B_EALREADY */                                                      \
    X(EALREADY,       B_EALREADY)                                       \
                                                                         \
    /* B_EINPROGRESS */                                                   \
    X(EINPROGRESS,    B_EINPROGRESS)                                    \
                                                                         \
    /* B_EMEDIUMTYPE */                                                   \
    X(EMEDIUMTYPE,    B_EMEDIUMTYPE)


constexpr int error_from_linux(int sys_errno) noexcept
{
    if (sys_errno <= 0)
        return sys_errno;

    switch (sys_errno) {

#define BUFFIO_LINUX_CASE(native, buffio) \
        case native: return buffio;

        BUFFIO_LINUX_ERROR_TABLE(BUFFIO_LINUX_CASE)

#undef BUFFIO_LINUX_CASE

        default:
            return B_EUNKNOWN;
    }
}

#endif // BUFFIO_OS_LINUX


// ============================================================================
// BSD errno -> BuffIO
// ============================================================================
//
// Keep BSD mappings independent from Linux. Different BSDs do not necessarily
// expose exactly the same errno extensions.
// ============================================================================

#if defined(BUFFIO_OS_BSD)

#define BUFFIO_BSD_ERROR_TABLE(X)                                        \
    X(EPERM,           B_EPERM)                                         \
    X(ENOENT,          B_ENOENT)                                        \
    X(ESRCH,           B_ESRCH)                                         \
    X(EINTR,           B_EINTR)                                         \
    X(EIO,             B_EIO)                                           \
    X(ENXIO,           B_ENXIO)                                         \
    X(E2BIG,           B_E2BIG)                                         \
    X(ENOEXEC,         B_ENOEXEC)                                       \
    X(EBADF,           B_EBADF)                                         \
    X(ECHILD,          B_ECHILD)                                        \
    X(EAGAIN,          B_EAGAIN)                                        \
    X(ENOMEM,          B_ENOMEM)                                        \
    X(EACCES,          B_EACCES)                                        \
    X(EFAULT,          B_EFAULT)                                         \
    X(EBUSY,           B_EBUSY)                                         \
    X(EEXIST,          B_EEXIST)                                        \
    X(EXDEV,           B_EXDEV)                                         \
    X(ENODEV,          B_ENODEV)                                        \
    X(ENOTDIR,         B_ENOTDIR)                                       \
    X(EISDIR,           B_EISDIR)                                       \
    X(EINVAL,           B_EINVAL)                                       \
    X(ENFILE,           B_ENFILE)                                       \
    X(EMFILE,           B_EMFILE)                                       \
    X(ENOTTY,           B_ENOTTY)                                       \
    X(EFBIG,            B_EFBIG)                                        \
    X(ENOSPC,           B_ENOSPC)                                       \
    X(ESPIPE,           B_ESPIPE)                                       \
    X(EROFS,            B_EROFS)                                        \
    X(EMLINK,           B_EMLINK)                                       \
    X(EPIPE,            B_EPIPE)                                        \
    X(EDOM,             B_EDOM)                                         \
    X(ERANGE,           B_ERANGE)                                       \
    X(EDEADLK,          B_EDEADLK)                                      \
    X(ENAMETOOLONG,     B_ENAMETOOLONG)                                \
    X(ENOLCK,           B_ENOLCK)                                       \
    X(ENOSYS,           B_ENOSYS)                                       \
    X(ENOTEMPTY,        B_ENOTEMPTY)                                    \
    X(ELOOP,             B_ELOOP)                                       \
    X(ENOTSOCK,          B_ENOTSOCK)                                    \
    X(EDESTADDRREQ,      B_EDESTADDRREQ)                               \
    X(EMSGSIZE,          B_EMSGSIZE)                                    \
    X(EPROTOTYPE,        B_EPROTOTYPE)                                  \
    X(ENOPROTOOPT,       B_ENOPROTOOPT)                                 \
    X(EPROTONOSUPPORT,   B_EPROTONOSUPPORT)                             \
    X(EOPNOTSUPP,        B_EOPNOTSUPP)                                  \
    X(EAFNOSUPPORT,      B_EAFNOSUPPORT)                                \
    X(EADDRINUSE,        B_EADDRINUSE)                                  \
    X(EADDRNOTAVAIL,     B_EADDRNOTAVAIL)                               \
    X(ENETDOWN,          B_ENETDOWN)                                     \
    X(ENETUNREACH,       B_ENETUNREACH)                                  \
    X(ENETRESET,         B_ENETRESET)                                    \
    X(ECONNABORTED,      B_ECONNABORTED)                                \
    X(ECONNRESET,        B_ECONNRESET)                                  \
    X(ENOBUFS,           B_ENOBUFS)                                     \
    X(EISCONN,           B_EISCONN)                                     \
    X(ENOTCONN,          B_ENOTCONN)                                    \
    X(ETIMEDOUT,         B_ETIMEDOUT)                                   \
    X(ECONNREFUSED,      B_ECONNREFUSED)                                \
    X(EHOSTUNREACH,      B_EHOSTUNREACH)                                \
    X(EALREADY,          B_EALREADY)                                    \
    X(EINPROGRESS,       B_EINPROGRESS)


constexpr int error_from_bsd(int sys_errno) noexcept
{
    if (sys_errno <= 0)
        return sys_errno;

    switch (sys_errno) {

#define BUFFIO_BSD_CASE(native, buffio) \
        case native: return buffio;

        BUFFIO_BSD_ERROR_TABLE(BUFFIO_BSD_CASE)

#undef BUFFIO_BSD_CASE

        default:
            return B_EUNKNOWN;
    }
}

#endif // BUFFIO_OS_BSD


// ============================================================================
// Windows native error -> BuffIO
// ============================================================================
//
// The table is deliberately grouped by the BuffIO error being produced.
//
// This is the same useful organization seen in libuv:
//
//     native error  --->  canonical library error
//
// Multiple native errors MAY map to one BuffIO error.
//
// A native error MUST NOT appear twice in this table, otherwise the generated
// switch would contain duplicate case labels.
//
// This table contains Win32 errors only.
// Winsock errors are handled separately below.
// ============================================================================

#if defined(BUFFIO_OS_WINDOWS)

#define BUFFIO_WINDOWS_ERROR_TABLE(X)                                  \
    /* B_EACCES */                                                     \
    X(WSAEACCES,                    B_EACCES)                           \
                                                                        \
    /* B_EADDRINUSE */                                                  \
    X(ERROR_ADDRESS_ALREADY_ASSOCIATED, B_EADDRINUSE)                  \
    X(WSAEADDRINUSE,                 B_EADDRINUSE)                     \
                                                                        \
    /* B_EADDRNOTAVAIL */                                               \
    X(WSAEADDRNOTAVAIL,              B_EADDRNOTAVAIL)                  \
                                                                        \
    /* B_EAFNOSUPPORT */                                                \
    X(WSAEAFNOSUPPORT,               B_EAFNOSUPPORT)                   \
                                                                        \
    /* B_EAGAIN */                                                      \
    X(WSAEWOULDBLOCK,                B_EAGAIN)                         \
    X(ERROR_NO_DATA,                  B_EAGAIN)                         \
                                                                        \
    /* B_EALREADY */                                                    \
    X(WSAEALREADY,                   B_EALREADY)                        \
                                                                        \
    /* B_EBADF */                                                       \
    X(ERROR_INVALID_FLAGS,            B_EBADF)                          \
    X(ERROR_INVALID_HANDLE,           B_EBADF)                          \
                                                                        \
    /* B_EBUSY */                                                       \
    X(ERROR_LOCK_VIOLATION,           B_EBUSY)                          \
    X(ERROR_PIPE_BUSY,                B_EBUSY)                          \
    X(ERROR_SHARING_VIOLATION,        B_EBUSY)                          \
                                                                        \
    /* B_ECANCELED */                                                   \
    X(ERROR_OPERATION_ABORTED,        B_EINTR)                          \
    X(WSAEINTR,                       B_EINTR)                          \
                                                                        \
    /* B_ECONNABORTED */                                                \
    X(ERROR_CONNECTION_ABORTED,       B_ECONNABORTED)                   \
    X(WSAECONNABORTED,                B_ECONNABORTED)                   \
                                                                        \
    /* B_ECONNREFUSED */                                                \
    X(ERROR_CONNECTION_REFUSED,       B_ECONNREFUSED)                  \
    X(WSAECONNREFUSED,                B_ECONNREFUSED)                  \
                                                                        \
    /* B_ECONNRESET */                                                  \
    X(ERROR_NETNAME_DELETED,           B_ECONNRESET)                   \
    X(WSAECONNRESET,                   B_ECONNRESET)                   \
                                                                        \
    /* B_EDESTADDRREQ */                                                \
    X(WSAEDESTADDRREQ,                B_EDESTADDRREQ)                   \
                                                                        \
    /* B_EDOM */                                                        \
    /* No direct Win32 equivalent. */                                   \
                                                                        \
    /* B_EEXIST */                                                      \
    X(ERROR_ALREADY_EXISTS,            B_EEXIST)                        \
    X(ERROR_FILE_EXISTS,               B_EEXIST)                        \
                                                                        \
    /* B_EFAULT */                                                      \
    X(ERROR_NOACCESS,                 B_EFAULT)                         \
    X(WSAEFAULT,                      B_EFAULT)                         \
                                                                        \
    /* B_EHOSTUNREACH */                                                \
    X(ERROR_HOST_UNREACHABLE,         B_EHOSTUNREACH)                  \
    X(WSAEHOSTUNREACH,                B_EHOSTUNREACH)                  \
                                                                        \
    /* B_EINVAL */                                                      \
    X(ERROR_INSUFFICIENT_BUFFER,      B_EINVAL)                        \
    X(ERROR_INVALID_DATA,             B_EINVAL)                        \
    X(ERROR_INVALID_PARAMETER,        B_EINVAL)                        \
    X(ERROR_SYMLINK_NOT_SUPPORTED,    B_EINVAL)                        \
    X(WSAEINVAL,                      B_EINVAL)                        \
    X(WSAEPFNOSUPPORT,                B_EINVAL)                        \
                                                                        \
    /* B_EIO */                                                         \
    X(ERROR_BEGINNING_OF_MEDIA,       B_EIO)                            \
    X(ERROR_BUS_RESET,                B_EIO)                            \
    X(ERROR_CRC,                      B_EIO)                            \
    X(ERROR_DEVICE_DOOR_OPEN,         B_EIO)                            \
    X(ERROR_DEVICE_REQUIRES_CLEANING, B_EIO)                            \
    X(ERROR_DISK_CORRUPT,             B_EIO)                            \
    X(ERROR_EOM_OVERFLOW,             B_EIO)                            \
    X(ERROR_FILEMARK_DETECTED,        B_EIO)                            \
    X(ERROR_GEN_FAILURE,              B_EIO)                            \
    X(ERROR_INVALID_BLOCK_LENGTH,     B_EIO)                            \
    X(ERROR_IO_DEVICE,                B_EIO)                            \
    X(ERROR_NO_DATA_DETECTED,         B_EIO)                            \
    X(ERROR_NO_SIGNAL_SENT,           B_EIO)                            \
    X(ERROR_OPEN_FAILED,              B_EIO)                            \
    X(ERROR_SETMARK_DETECTED,         B_EIO)                            \
    X(ERROR_SIGNAL_REFUSED,           B_EIO)                            \
                                                                        \
    /* B_EISCONN */                                                     \
    X(WSAEISCONN,                     B_EISCONN)                        \
                                                                        \
    /* B_ELOOP */                                                       \
    X(ERROR_CANT_RESOLVE_FILENAME,    B_ELOOP)                          \
                                                                        \
    /* B_EMFILE */                                                      \
    X(ERROR_TOO_MANY_OPEN_FILES,      B_EMFILE)                         \
    X(WSAEMFILE,                      B_EMFILE)                          \
                                                                        \
    /* B_EMSGSIZE */                                                    \
    X(WSAEMSGSIZE,                    B_EMSGSIZE)                       \
                                                                        \
    /* B_ENAMETOOLONG */                                                \
    X(ERROR_BUFFER_OVERFLOW,          B_ENAMETOOLONG)                   \
    X(ERROR_FILENAME_EXCED_RANGE,     B_ENAMETOOLONG)                   \
                                                                        \
    /* B_ENETUNREACH */                                                 \
    X(ERROR_NETWORK_UNREACHABLE,      B_ENETUNREACH)                    \
    X(WSAENETUNREACH,                 B_ENETUNREACH)                    \
                                                                        \
    /* B_ENOBUFS */                                                     \
    X(WSAENOBUFS,                     B_ENOBUFS)                        \
                                                                        \
    /* B_ENOENT */                                                      \
    X(ERROR_BAD_PATHNAME,             B_ENOENT)                         \
    X(ERROR_DIRECTORY,                B_ENOENT)                         \
    X(ERROR_ENVVAR_NOT_FOUND,         B_ENOENT)                         \
    X(ERROR_FILE_NOT_FOUND,           B_ENOENT)                         \
    X(ERROR_INVALID_NAME,             B_ENOENT)                         \
    X(ERROR_INVALID_DRIVE,            B_ENOENT)                         \
    X(ERROR_INVALID_REPARSE_DATA,     B_ENOENT)                         \
    X(ERROR_MOD_NOT_FOUND,            B_ENOENT)                         \
    X(ERROR_PATH_NOT_FOUND,           B_ENOENT)                         \
    X(WSAHOST_NOT_FOUND,              B_ENOENT)                         \
    X(WSANO_DATA,                     B_ENOENT)                         \
                                                                        \
    /* B_ENOMEM */                                                      \
    X(ERROR_NOT_ENOUGH_MEMORY,         B_ENOMEM)                        \
    X(ERROR_OUTOFMEMORY,               B_ENOMEM)                        \
                                                                        \
    /* B_ENOSPC */                                                      \
    X(ERROR_CANNOT_MAKE,               B_ENOSPC)                        \
    X(ERROR_DISK_FULL,                 B_ENOSPC)                        \
    X(ERROR_EA_TABLE_FULL,             B_ENOSPC)                        \
    X(ERROR_END_OF_MEDIA,              B_ENOSPC)                        \
    X(ERROR_HANDLE_DISK_FULL,          B_ENOSPC)                        \
                                                                        \
    /* B_ENOTCONN */                                                    \
    X(ERROR_NOT_CONNECTED,             B_ENOTCONN)                      \
    X(WSAENOTCONN,                     B_ENOTCONN)                      \
                                                                        \
    /* B_ENOTEMPTY */                                                   \
    X(ERROR_DIR_NOT_EMPTY,             B_ENOTEMPTY)                     \
                                                                        \
    /* B_ENOTSOCK */                                                    \
    X(WSAENOTSOCK,                     B_ENOTSOCK)                      \
                                                                        \
    /* B_EOPNOTSUPP */                                                  \
    X(ERROR_NOT_SUPPORTED,              B_EOPNOTSUPP)                   \
                                                                        \
    /* B_EPERM */                                                       \
    X(ERROR_ACCESS_DENIED,              B_EPERM)                        \
    X(ERROR_ELEVATION_REQUIRED,         B_EPERM)                        \
    X(ERROR_CANT_ACCESS_FILE,           B_EPERM)                        \
    X(ERROR_PRIVILEGE_NOT_HELD,         B_EPERM)                        \
                                                                        \
    /* B_EPIPE */                                                       \
    X(ERROR_BAD_PIPE,                   B_EPIPE)                        \
    X(ERROR_PIPE_NOT_CONNECTED,         B_EPIPE)                        \
    X(WSAESHUTDOWN,                     B_EPIPE)                        \
                                                                        \
    /* B_EPROTONOSUPPORT */                                             \
    X(WSAEPROTONOSUPPORT,               B_EPROTONOSUPPORT)              \
                                                                        \
    /* B_EROFS */                                                       \
    X(ERROR_WRITE_PROTECT,              B_EROFS)                        \
                                                                        \
    /* B_ESOCKTNOSUPPORT */                                             \
    X(WSAESOCKTNOSUPPORT,               B_EOPNOTSUPP)                   \
                                                                        \
    /* B_ETIMEDOUT */                                                   \
    X(ERROR_SEM_TIMEOUT,                B_ETIMEDOUT)                    \
    X(WSAETIMEDOUT,                     B_ETIMEDOUT)                    \
                                                                        \
    /* B_EXDEV */                                                       \
    X(ERROR_NOT_SAME_DEVICE,            B_EXDEV)                        \
                                                                        \
    /* B_E2BIG */                                                       \
    X(ERROR_META_EXPANSION_TOO_LONG,    B_E2BIG)                        \
                                                                        \
    /* B_EISDIR */                                                      \
    X(ERROR_INVALID_FUNCTION,           B_EISDIR)                       \
                                                                        \
    /* B_EFTYPE / unsupported executable format */                      \
    X(ERROR_BAD_EXE_FORMAT,             B_ENOEXEC)                      \
                                                                        \
    /* B_EOF */                                                         \
    X(ERROR_BROKEN_PIPE,                B_EPIPE)


// ============================================================================
// Windows translation
// ============================================================================

constexpr int error_from_windows(DWORD sys_error) noexcept
{
    switch (sys_error) {

#define BUFFIO_WINDOWS_CASE(native, buffio) \
        case native: return buffio;

        BUFFIO_WINDOWS_ERROR_TABLE(BUFFIO_WINDOWS_CASE)

#undef BUFFIO_WINDOWS_CASE

        default:
            return B_EUNKNOWN;
    }
}

#endif // BUFFIO_OS_WINDOWS


// ============================================================================
// Generic native OS -> BuffIO translation
// ============================================================================
//
// The argument is expected to be the native error value:
//
// Linux/BSD:
//     errno
//
// Windows:
//     GetLastError()
//
// If a negative value is supplied, it is treated as an already-translated
// BuffIO error. This follows the useful property of libuv's translation API:
// translating an already canonicalized error is harmless.
// ============================================================================

constexpr int error_from_os(int error) noexcept
{
    if (error <= 0)
        return error;

#if defined(BUFFIO_OS_WINDOWS)

    return error_from_windows(static_cast<DWORD>(error));

#elif defined(BUFFIO_OS_LINUX)

    return error_from_linux(error);

#elif defined(BUFFIO_OS_BSD)

    return error_from_bsd(error);

#else

    (void)error;
    return B_EUNKNOWN;

#endif
}

} // namespace buffio
