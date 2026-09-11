#include "buffio/core.hpp"
#include <cstring>

bool buffio::BuffervState::CreateVec(int num){
 assert(num > 0 && io_vecs == nullptr && num <= 10);

 buffio::Bufferiov *iov = new (std::nothrow) buffio::Bufferiov[num];

 if(iov == nullptr) return false;
 memset((char *)iov,'\0',sizeof(buffio::Bufferiov) * num);
 
 io_vecs = iov;
 size_max = num;
 iown = true;

 return true;
};

bool buffio::BuffervState::MakeEntry(int idx, char *buffer, 
                           size_t bufSize){


  assert(idx <= size_max && io_vecs != nullptr);
  assert(size != size_max);

  buffio::Bufferiov *iov = io_vecs;
   
  iov = (iov + (idx - 1));
  *iov = buffer;
  *iov = bufSize;

  /*
  #if defined(BUFFIO_OS_LINUX)
   iov->iov_base = static_cast<void*>(buffer);
   iov->iov_len = bufSize;
  #else
   iov->buffer = buffer;
   iov->size = bufSize;
  #endif
  */

  size += 1;
  
  return true;
};
