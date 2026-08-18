#ifndef BUFFIO_WORKER
#define BUFFIO_WORKER

#include "buffio/defs.hpp"
#include "buffio/macro.hpp"
#include <atomic>

namespace buffio{
class worker{
  public:
    BUFFIO_CLASS_PROTECT(worker);
    int init(int numWorker);
    void pushWork(BGLOBOPVEC vec){}
    buffio::vTask pullResume(){return nullptr;}
    ~worker();
     worker();
  private:

  void *workerControl;
};
};
#endif
