#ifndef BUFFIO_OS
#define BUFFIO_OS

#if defined(__linux__) || defined(__gnu_linux__)

#include <utility>
#include <cstdint>
#include <cstddef>
#include <errno.h>
#include <sys/types.h>

#define BUFFIO_OS_LINUX 1

#define BUFFIO_OS_INSERT(lin,bsd,wind) lin
#define BUFFIO_LIN_INSERT(codeSection) codeSection

#define BUFFIO_WIN_INSERT(codeSection)

#elif defined(_MSC_VER) || defined(_WIN32)

#define BUFFIO_OS_WINDOWS 1
#include <windows.h>
#include <BaseTsd.h>

typedef SSIZE_T ssize_t;
typedef SIZE_T size_t;


#define BUFFIO_WIN_INSERT(codeSection) codeSection
#define BUFFIO_OS_INSERT(lin,bsd,wind) wind

#define BUFFIO_LIN_INSERT(codeSection) codeSection
#endif

#define BUFFIO_FILE_VEC_MAX 10

using BFLAG = int;
using BMODE = uint32_t;
using BFDDEF = BUFFIO_OS_INSERT(int,int,uintptr_t);
constexpr int BFD_INVALID = -1;

#endif
