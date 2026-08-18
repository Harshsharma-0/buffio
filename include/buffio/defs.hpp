#ifndef BUFFIO_DEFS
#define BUFFIO_DEFS

#include "buffio/optable.hpp"

typedef struct BFILE{
 BFDDEF fd;
 int flags;
 void *currRq;
 }BFILE, *PBFILE;

typedef struct BSOCKET{
 BFDDEF fd;
 int flags;
 void *currRq;
}BSOCKET,*PBSOCKET;


using BFFDOPT = std::pair<BFILE,int>;
using BFRWOPT = std::pair<ssize_t,int>;
using BFOPT   = std::pair<int,int>;
using BFSRWOPT = std::pair<ssize_t,uintptr_t>;

typedef struct fileOpVec {
  char *buffer;
  size_t len;
} fileOpVec;


#endif
