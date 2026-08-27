#ifndef BUFFIO_E_CODE_HPP
#define BUFFIO_E_CODE_HPP

#include "buffio/config.hpp"

#include <cerrno>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #include <windows.h>
    #include <winsock2.h>
#endif


/*
 * ============================================================================
 * Platform detection
 * ============================================================================
 */

#if defined(__linux__)

    #define BUFFIO_OS_LINUX 1
    #define BUFFIO_OS_BSD   0
    #define BUFFIO_OS_WINDOWS 0

#elif defined(__FreeBSD__) || defined(__OpenBSD__) || \
      defined(__NetBSD__) || defined(__DragonFly__)

    #define BUFFIO_OS_LINUX 0
    #define BUFFIO_OS_BSD   1
    #define BUFFIO_OS_WINDOWS 0

#elif defined(_WIN32)

    #define BUFFIO_OS_LINUX 0
    #define BUFFIO_OS_BSD   0
    #define BUFFIO_OS_WINDOWS 1

#else

    #error "BuffIO: unsupported operating system"

#endif


/*
 * ============================================================================
 * Platform selector
 * ============================================================================
 *
 * Usage:
 *
 *     BUFFIO_OS_INSERT(EPERM, EPERM, ERROR_ACCESS_DENIED)
 *
 * Linux:
 *     -> EPERM
 *
 * BSD:
 *     -> EPERM
 *
 * Windows:
 *     -> ERROR_ACCESS_DENIED
 */

#if BUFFIO_OS_LINUX

    #define BUFFIO_OS_INSERT(linux_field, bsd_field, windows_field) linux_field

#elif BUFFIO_OS_BSD

    #define BUFFIO_OS_INSERT(linux_field, bsd_field, windows_field) bsd_field

#elif BUFFIO_OS_WINDOWS

    #define BUFFIO_OS_INSERT(linux_field, bsd_field, windows_field) windows_field

#endif


/*
 * ============================================================================
 * BuffIO error table
 * ============================================================================
 *
 * Format:
 *
 * BUFFIO_ERROR(
 *     NAME,
 *     LINUX_ERRNO,
 *     BSD_ERRNO,
 *     WINDOWS_ERROR,
 *     "description"
 * )
 *
 * IMPORTANT:
 *
 * The Linux value is the canonical numeric value.
 *
 * Therefore:
 *
 *     EPERM  = 1
 *     B_EPERM = -1
 *
 *     ENOENT  = 2
 *     B_ENOENT = -2
 *
 * etc.
 *
 * The native BSD/Windows values are ONLY used for translation.
 */

#define BUFFIO_ERROR_TABLE(X)                                             \
                                                                          \
    X(EPERM,                                                              \
      EPERM,                                                              \
      EPERM,                                                              \
      ERROR_ACCESS_DENIED,                                                \
      "Operation not permitted")                                          \
                                                                          \
    X(ENOENT,                                                             \
      ENOENT,                                                             \
      ENOENT,                                                             \
      ERROR_FILE_NOT_FOUND,                                               \
      "No such file or directory")                                        \
                                                                          \
    X(ESRCH,                                                              \
      ESRCH,                                                              \
      ESRCH,                                                              \
      ERROR_INVALID_PARAMETER,                                            \
      "No such process")                                                  \
                                                                          \
    X(EINTR,                                                              \
      EINTR,                                                              \
      EINTR,                                                              \
      ERROR_OPERATION_ABORTED,                                            \
      "Interrupted system call")                                          \
                                                                          \
    X(EIO,                                                                \
      EIO,                                                                \
      EIO,                                                                \
      ERROR_IO_DEVICE,                                                    \
      "Input/output error")                                               \
                                                                          \
    X(ENXIO,                                                              \
      ENXIO,                                                              \
      ENXIO,                                                              \
      ERROR_DEV_NOT_EXIST,                                                \
      "No such device or address")                                        \
                                                                          \
    X(E2BIG,                                                              \
      E2BIG,                                                              \
      E2BIG,                                                              \
      ERROR_BAD_ARGUMENTS,                                                \
      "Argument list too long")                                           \
                                                                          \
    X(ENOEXEC,                                                            \
      ENOEXEC,                                                            \
      ENOEXEC,                                                            \
      ERROR_BAD_EXE_FORMAT,                                               \
      "Exec format error")                                                \
                                                                          \
    X(EBADF,                                                              \
      EBADF,                                                              \
      EBADF,                                                              \
      ERROR_INVALID_HANDLE,                                               \
      "Bad file descriptor")                                              \
                                                                          \
    X(ECHILD,                                                             \
      ECHILD,                                                             \
      ECHILD,                                                             \
      ERROR_CHILD_NOT_COMPLETE,                                           \
      "No child processes")                                               \
                                                                          \
    X(EAGAIN,                                                             \
      EAGAIN,                                                             \
      EAGAIN,                                                             \
      ERROR_RETRY,                                                        \
      "Resource temporarily unavailable")                                 \
                                                                          \
    X(ENOMEM,                                                             \
      ENOMEM,                                                             \
      ENOMEM,                                                             \
      ERROR_NOT_ENOUGH_MEMORY,                                            \
      "Out of memory")                                                     \
                                                                          \
    X(EACCES,                                                             \
      EACCES,                                                             \
      EACCES,                                                             \
      ERROR_ACCESS_DENIED,                                                \
      "Permission denied")                                                \
                                                                          \
    X(EFAULT,                                                             \
      EFAULT,                                                             \
      EFAULT,                                                             \
      ERROR_INVALID_ADDRESS,                                              \
      "Bad address")                                                      \
                                                                          \
    X(EBUSY,                                                              \
      EBUSY,                                                              \
      EBUSY,                                                              \
      ERROR_BUSY,                                                         \
      "Device or resource busy")                                          \
                                                                          \
    X(EEXIST,                                                             \
      EEXIST,                                                             \
      EEXIST,                                                             \
      ERROR_FILE_EXISTS,                                                  \
      "File exists")                                                      \
                                                                          \
    X(EXDEV,                                                              \
      EXDEV,                                                              \
      EXDEV,                                                              \
      ERROR_NOT_SAME_DEVICE,                                              \
      "Invalid cross-device link")                                        \
                                                                          \
    X(ENODEV,                                                             \
      ENODEV,                                                             \
      ENODEV,                                                             \
      ERROR_DEV_NOT_EXIST,                                                \
      "No such device")                                                   \
                                                                          \
    X(ENOTDIR,                                                            \
      ENOTDIR,                                                            \
      ENOTDIR,                                                            \
      ERROR_DIRECTORY,                                                    \
      "Not a directory")                                                  \
                                                                          \
    X(EISDIR,                                                             \
      EISDIR,                                                             \
      EISDIR,                                                             \
      ERROR_DIRECTORY,                                                    \
      "Is a directory")                                                   \
                                                                          \
    X(EINVAL,                                                             \
      EINVAL,                                                             \
      EINVAL,                                                             \
      ERROR_INVALID_PARAMETER,                                             \
      "Invalid argument")                                                 \
                                                                          \
    X(ENFILE,                                                             \
      ENFILE,                                                              \
      ENFILE,                                                              \
      ERROR_TOO_MANY_OPEN_FILES,                                          \
      "Too many open files in system")                                    \
                                                                          \
    X(EMFILE,                                                             \
      EMFILE,                                                              \
      EMFILE,                                                              \
      ERROR_TOO_MANY_OPEN_FILES,                                          \
      "Too many open files")                                              \
                                                                          \
    X(ENOSPC,                                                             \
      ENOSPC,                                                              \
      ENOSPC,                                                              \
      ERROR_DISK_FULL,                                                    \
      "No space left on device")                                          \
                                                                          \
    X(ESPIPE,                                                             \
      ESPIPE,                                                              \
      ESPIPE,                                                              \
      ERROR_SEEK,                                                         \
      "Illegal seek")                                                     \
                                                                          \
    X(EROFS,                                                              \
      EROFS,                                                              \
      EROFS,                                                              \
      ERROR_WRITE_PROTECT,                                                \
      "Read-only file system")                                            \
                                                                          \
    X(EMLINK,                                                             \
      EMLINK,                                                              \
      EMLINK,                                                              \
      ERROR_TOO_MANY_LINKS,                                               \
      "Too many links")                                                   \
                                                                          \
    X(EPIPE,                                                              \
      EPIPE,                                                              \
      EPIPE,                                                              \
      ERROR_BROKEN_PIPE,                                                  \
      "Broken pipe")                                                      \
                                                                          \
    X(EDOM,                                                               \
      EDOM,                                                               \
      EDOM,                                                               \
      ERROR_DOMAIN,                                                       \
      "Numerical argument out of domain")                                 \
                                                                          \
    X(ERANGE,                                                             \
      ERANGE,                                                             \
      ERANGE,                                                             \
      ERROR_ARITHMETIC_OVERFLOW,                                          \
      "Numerical result out of range")                                    \
                                                                          \
    X(EDEADLK,                                                            \
      EDEADLK,                                                             \
      EDEADLK,                                                             \
      ERROR_POSSIBLE_DEADLOCK,                                            \
      "Resource deadlock would occur")                                    \
                                                                          \
    X(ENAMETOOLONG,                                                        \
      ENAMETOOLONG,                                                        \
      ENAMETOOLONG,                                                        \
      ERROR_FILENAME_EXCED_RANGE,                                         \
      "File name too long")                                               \
                                                                          \
    X(ENOLCK,                                                             \
      ENOLCK,                                                              \
      ENOLCK,                                                              \
      ERROR_LOCK_VIOLATION,                                               \
      "No record locks available")                                        \
                                                                          \
    X(ENOSYS,                                                             \
      ENOSYS,                                                              \
      ENOSYS,                                                              \
      ERROR_INVALID_FUNCTION,                                             \
      "Function not implemented")                                         \
                                                                          \
    X(ENOTEMPTY,                                                          \
      ENOTEMPTY,                                                           \
      ENOTEMPTY,                                                           \
      ERROR_DIR_NOT_EMPTY,                                                \
      "Directory not empty")                                              \
                                                                          \
    X(ELOOP,                                                              \
      ELOOP,                                                               \
      ELOOP,                                                               \
      ERROR_CANT_RESOLVE_FILENAME,                                        \
      "Too many levels of symbolic links")                               \
                                                                          \
    X(ENOMSG,                                                             \
      ENOMSG,                                                              \
      ENOMSG,                                                              \
      ERROR_INVALID_MESSAGE,                                              \
      "No message of desired type")                                       \
                                                                          \
    X(EIDRM,                                                              \
      EIDRM,                                                               \
      EIDRM,                                                               \
      ERROR_INVALID_HANDLE,                                               \
      "Identifier removed")                                               \
                                                                          \
    X(EILSEQ,                                                             \
      EILSEQ,                                                              \
      EILSEQ,                                                              \
      ERROR_NO_UNICODE_TRANSLATION,                                       \
      "Illegal byte sequence")                                            \
                                                                          \
    X(ENOTSOCK,                                                           \
      ENOTSOCK,                                                            \
      ENOTSOCK,                                                            \
      WSAENOTSOCK,                                                         \
      "Socket operation on non-socket")                                   \
                                                                          \
    X(EDESTADDRREQ,                                                        \
      EDESTADDRREQ,                                                        \
      EDESTADDRREQ,                                                        \
      WSAEDESTADDRREQ,                                                     \
      "Destination address required")                                     \
                                                                          \
    X(EMSGSIZE,                                                           \
      EMSGSIZE,                                                            \
      EMSGSIZE,                                                            \
      WSAEMSGSIZE,                                                         \
      "Message too long")                                                 \
                                                                          \
    X(EPROTOTYPE,                                                         \
      EPROTOTYPE,                                                          \
      EPROTOTYPE,                                                          \
      WSAEPROTOTYPE,                                                       \
      "Protocol wrong type for socket")                                   \
                                                                          \
    X(ENOPROTOOPT,                                                        \
      ENOPROTOOPT,                                                         \
      ENOPROTOOPT,                                                         \
      WSAENOPROTOOPT,                                                      \
      "Protocol not available")                                           \
                                                                          \
    X(EPROTONOSUPPORT,                                                     \
      EPROTONOSUPPORT,                                                      \
      EPROTONOSUPPORT,                                                      \
      WSAEPROTONOSUPPORT,                                                  \
      "Protocol not supported")                                           \
                                                                          \
    X(ESOCKTNOSUPPORT,                                                      \
      ESOCKTNOSUPPORT,                                                      \
      ESOCKTNOSUPPORT,                                                      \
      WSAESOCKTNOSUPPORT,                                                  \
      "Socket type not supported")                                        \
                                                                          \
    X(EOPNOTSUPP,                                                          \
      EOPNOTSUPP,                                                           \
      EOPNOTSUPP,                                                           \
      WSAEOPNOTSUPP,                                                        \
      "Operation not supported")                                          \
                                                                          \
    X(EPFNOSUPPORT,                                                        \
      EPFNOSUPPORT,                                                         \
      EPFNOSUPPORT,                                                         \
      WSAEPFNOSUPPORT,                                                     \
      "Protocol family not supported")                                    \
                                                                          \
    X(EAFNOSUPPORT,                                                        \
      EAFNOSUPPORT,                                                         \
      EAFNOSUPPORT,                                                         \
      WSAEAFNOSUPPORT,                                                     \
      "Address family not supported by protocol")                         \
                                                                          \
    X(EADDRINUSE,                                                          \
      EADDRINUSE,                                                           \
      EADDRINUSE,                                                           \
      WSAEADDRINUSE,                                                        \
      "Address already in use")                                           \
                                                                          \
    X(EADDRNOTAVAIL,                                                        \
      EADDRNOTAVAIL,                                                         \
      EADDRNOTAVAIL,                                                         \
      WSAEADDRNOTAVAIL,                                                     \
      "Cannot assign requested address")                                   \
                                                                          \
    X(ENETDOWN,                                                            \
      ENETDOWN,                                                             \
      ENETDOWN,                                                             \
      WSAENETDOWN,                                                          \
      "Network is down")                                                   \
                                                                          \
    X(ENETUNREACH,                                                         \
      ENETUNREACH,                                                          \
      ENETUNREACH,                                                          \
      WSAENETUNREACH,                                                       \
      "Network is unreachable")                                            \
                                                                          \
    X(ENETRESET,                                                           \
      ENETRESET,                                                            \
      ENETRESET,                                                            \
      WSAENETRESET,                                                         \
      "Network dropped connection because of reset")                       \
                                                                          \
    X(ECONNABORTED,                                                        \
      ECONNABORTED,                                                         \
      ECONNABORTED,                                                         \
      WSAECONNABORTED,                                                      \
      "Software caused connection abort")                                 \
                                                                          \
    X(ECONNRESET,                                                          \
      ECONNRESET,                                                           \
      ECONNRESET,                                                           \
      WSAECONNRESET,                                                        \
      "Connection reset by peer")                                         \
                                                                          \
    X(ENOBUFS,                                                             \
      ENOBUFS,                                                              \
      ENOBUFS,                                                              \
      WSAENOBUFS,                                                           \
      "No buffer space available")                                         \
                                                                          \
    X(EISCONN,                                                             \
      EISCONN,                                                              \
      EISCONN,                                                              \
      WSAEISCONN,                                                           \
      "Transport endpoint is already connected")                          \
                                                                          \
    X(ENOTCONN,                                                            \
      ENOTCONN,                                                             \
      ENOTCONN,                                                             \
      WSAENOTCONN,                                                          \
      "Transport endpoint is not connected")                              \
                                                                          \
    X(ESHUTDOWN,                                                           \
      ESHUTDOWN,                                                            \
      ESHUTDOWN,                                                            \
      WSAESHUTDOWN,                                                         \
      "Cannot send after transport endpoint shutdown")                    \
                                                                          \
    X(ETIMEDOUT,                                                           \
      ETIMEDOUT,                                                            \
      ETIMEDOUT,                                                            \
      WSAETIMEDOUT,                                                         \
      "Connection timed out")                                              \
                                                                          \
    X(ECONNREFUSED,                                                        \
      ECONNREFUSED,                                                         \
      ECONNREFUSED,                                                         \
      WSAECONNREFUSED,                                                      \
      "Connection refused")                                               \
                                                                          \
    X(EHOSTDOWN,                                                           \
      EHOSTDOWN,                                                            \
      EHOSTDOWN,                                                            \
      WSAEHOSTDOWN,                                                         \
      "Host is down")                                                      \
                                                                          \
    X(EHOSTUNREACH,                                                        \
      EHOSTUNREACH,                                                         \
      EHOSTUNREACH,                                                         \
      WSAEHOSTUNREACH,                                                      \
      "No route to host")                                                  \
                                                                          \
    X(EALREADY,                                                            \
      EALREADY,                                                             \
      EALREADY,                                                             \
      WSAEALREADY,                                                          \
      "Operation already in progress")                                    \
                                                                          \
    X(EINPROGRESS,                                                         \
      EINPROGRESS,                                                          \
      EINPROGRESS,                                                          \
      WSAEINPROGRESS,                                                       \
      "Operation now in progress")                                        \
                                                                          \
    X(ESTALE,                                                              \
      ESTALE,                                                               \
      ESTALE,                                                               \
      ERROR_INVALID_HANDLE,                                                \
      "Stale file handle")                                                 \
                                                                          \
    X(EDQUOT,                                                              \
      EDQUOT,                                                               \
      EDQUOT,                                                               \
      ERROR_DISK_QUOTA_EXCEEDED,                                           \
      "Quota exceeded")                                                    \
                                                                          \
    X(ENOMEDIUM,                                                           \
      ENOMEDIUM,                                                            \
      ENOMEDIUM,                                                            \
      ERROR_NO_MEDIA_IN_DEVICE,                                            \
      "No medium found")                                                   \
                                                                          \
    X(EMEDIUMTYPE,                                                         \
      EMEDIUMTYPE,                                                          \
      EMEDIUMTYPE,                                                          \
      ERROR_UNRECOGNIZED_MEDIA,                                            \
      "Wrong medium type")                                                 \
                                                                          \
    X(ECANCELED,                                                           \
      ECANCELED,                                                            \
      ECANCELED,                                                            \
      ERROR_CANCELLED,                                                      \
      "Operation canceled")                                                \
                                                                          \
    X(ENOKEY,                                                              \
      ENOKEY,                                                               \
      ENOKEY,                                                               \
      ERROR_INVALID_PASSWORD,                                               \
      "Required key not available")                                       \
                                                                          \
    X(EKEYEXPIRED,                                                         \
      EKEYEXPIRED,                                                          \
      EKEYEXPIRED,                                                          \
      ERROR_PASSWORD_EXPIRED,                                               \
      "Key has expired")                                                   \
                                                                          \
    X(EKEYREVOKED,                                                         \
      EKEYREVOKED,                                                          \
      EKEYREVOKED,                                                          \
      ERROR_PASSWORD_RESTRICTION,                                           \
      "Key has been revoked")                                              \
                                                                          \
    X(EKEYREJECTED,                                                        \
      EKEYREJECTED,                                                         \
      EKEYREJECTED,                                                         \
      ERROR_ACCESS_DENIED,                                                  \
      "Key was rejected by service")                                       \
                                                                          \
    X(EOWNERDEAD,                                                          \
      EOWNERDEAD,                                                           \
      EOWNERDEAD,                                                           \
      ERROR_PROCESS_ABORTED,                                                \
      "Owner died")                                                        \
                                                                          \
    X(ENOTRECOVERABLE,                                                     \
      ENOTRECOVERABLE,                                                      \
      ENOTRECOVERABLE,                                                      \
      ERROR_UNRECOVERABLE_ERROR,                                            \
      "State not recoverable")                                             \
                                                                          \
    X(ERFKILL,                                                             \
      ERFKILL,                                                              \
      ERFKILL,                                                              \
      ERROR_DEVICE_NOT_AVAILABLE,                                           \
      "Operation not possible due to RF-kill")                             \
                                                                          \
    X(EHWPOISON,                                                           \
      EHWPOISON,                                                            \
      EHWPOISON,                                                            \
      ERROR_HW_MALFUNCTION,                                                 \
      "Memory page has hardware error")


/*
 * ============================================================================
 * Generate BuffIO error constants
 * ============================================================================
 *
 * Linux is the ABI/canonical numbering.
 *
 * B_EPERM  == -EPERM
 * B_ENOENT == -ENOENT
 */

#define BUFFIO_DEFINE_ERROR(name, linux_errno, bsd_errno, windows_error, msg) \
    constexpr int B_##name = -(linux_errno);

BUFFIO_ERROR_TABLE(BUFFIO_DEFINE_ERROR)

#undef BUFFIO_DEFINE_ERROR


/*
 * ============================================================================
 * Unknown error
 * ============================================================================
 */

constexpr int B_EUNKNOWN = -0x7fff;


/*
 * ============================================================================
 * strerror()
 * ============================================================================
 *
 * Similar purpose to strerror(), but for BuffIO's canonical error codes.
 *
 * Example:
 *
 *     buffio::strerror(B_ENOENT)
 *
 * returns:
 *
 *     "B_ENOENT: No such file or directory"
 */

constexpr const char* strerror(int error) noexcept
{
    switch (error) {

#define BUFFIO_ERROR_STRING(name, linux_errno, bsd_errno, windows_error, msg) \
        case B_##name:                                                        \
            return "B_" #name ": " msg;

        BUFFIO_ERROR_TABLE(BUFFIO_ERROR_STRING)

#undef BUFFIO_ERROR_STRING

        default:
            return "B_EUNKNOWN: Unknown BuffIO error";
    }
}


/*
 * ============================================================================
 * Native OS error -> BuffIO error
 * ============================================================================
 *
 * On POSIX:
 *
 *     errno = positive native errno
 *
 * BuffIO:
 *
 *     B_E* = negative Linux errno
 *
 * Therefore:
 *
 *     errno == EPERM
 *         |
 *         v
 *     B_EPERM == -EPERM
 *
 * On Windows, the native value can be a Win32 ERROR_* or Winsock WSA* value.
 */

constexpr int error_from_os(int native_error) noexcept
{
    switch (native_error) {

#define BUFFIO_ERROR_FROM_OS(name, linux_errno, bsd_errno, windows_error, msg) \
        case BUFFIO_OS_INSERT(                                      \
            linux_errno,                                           \
            bsd_errno,                                             \
            windows_error):                                        \
            return B_##name;

        BUFFIO_ERROR_TABLE(BUFFIO_ERROR_FROM_OS)

#undef BUFFIO_ERROR_FROM_OS

        default:
            return B_EUNKNOWN;
    }
}


/*
 * ============================================================================
 * Optional helper: convert a negative BuffIO error back to native OS error
 * ============================================================================
 *
 * This is useful when BuffIO needs to call another OS API.
 */

constexpr int os_error(int buffio_error) noexcept
{
    switch (buffio_error) {

#define BUFFIO_ERROR_TO_OS(name, linux_errno, bsd_errno, windows_error, msg) \
        case B_##name:                                                       \
            return BUFFIO_OS_INSERT(                                         \
                linux_errno,                                                 \
                bsd_errno,                                                   \
                windows_error);

        BUFFIO_ERROR_TABLE(BUFFIO_ERROR_TO_OS)

#undef BUFFIO_ERROR_TO_OS

        default:
            return 0;
    }
}

} // namespace buffio


/*
 * ============================================================================
 * Cleanup
 * ============================================================================
 */

#undef BUFFIO_OS_INSERT

#undef BUFFIO_OS_LINUX
#undef BUFFIO_OS_BSD
#undef BUFFIO_OS_WINDOWS

#endif // BUFFIO_E_CODE_HPP

