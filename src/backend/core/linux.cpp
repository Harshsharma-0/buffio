#include "buffio/core.hpp"
#include <cstring>

bool buffio::BuffervState::CreateVec(int num){
 assert(num > 0 && io_vecs == nullptr && num <= 10);

 struct iovec *iov = new (std::nothrow)
                       struct iovec[num];
 if(iov == nullptr) return false;
 memset((char *)iov,'\0',sizeof(struct iovec) * num);
 
 io_vecs = iov;
 size_max = num;
 iown = true;

 return true;
};

bool buffio::BuffervState::MakeEntry(int idx, char *buffer, 
                           size_t bufSize){


  assert(idx <= size_max && io_vecs != nullptr);
  assert(size != size_max);

  struct iovec *iov = io_vecs;
   
  iov = (iov + (idx - 1));
  iov->iov_base = static_cast<void*>(buffer);
  iov->iov_len = bufSize;

  size += 1;
  
  return true;
};
