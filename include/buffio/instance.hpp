#ifndef BUFFIO_INSTANCE
#define BUFFIO_INSTANCE

#include "buffio/macro.hpp"
#include "buffio/task.hpp"
#include "buffio/worker.hpp"

namespace buffio {

class instance : private buffio::worker{
public:
  BUFFIO_CLASS_PROTECT(instance)
  instance();

  int init();
  int start();
  
  int push(buffio::vTask task) {
    runQueue.push(task);
    return 1;
  };

  int run() {
    buffio::vTask task;
    while (!runQueue.empty()) {
      if ((task = runQueue.pop(nullptr))) {
        task.resume();
        continue;
      }
      std::cout << "invalid entry" << std::endl;
      return -1;
    }
    return 1;
  }

  ~instance() = default;

  friend class core::instance;

private:
  taskQueueDef runQueue;
};
} // namespace buffio

#endif
