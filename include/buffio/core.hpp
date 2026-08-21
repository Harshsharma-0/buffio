#ifndef BUFFIO_CORE
#define BUFFIO_CORE
#include <cassert>
#include <coroutine>
#include <utility>

/* BUFFIO FORWARD DECLARED */
namespace buffio{
class Worker;
using instance = Worker;
};

/* BUFFIO TASK DECLARTION */
namespace buffio {

template <typename promiseT> class promise;
using vTask = std::coroutine_handle<>;

struct promise_packed {
  buffio::instance *instance;
  buffio::vTask waiter;
  bool waiterAvailable; 
};

};

/* BUFFIO CORE */
namespace buffio{
namespace core {
class awaitable {};
class promise {};
class instance {};
struct task{ 
 bool core_schedule(buffio::instance &_instance,buffio::vTask task);
 bool core_promise_and_push(promise_packed &promise,
                   buffio::vTask task,buffio::vTask self);
};
}; // namespace core
};

/* BUFFIO FILEOPS FORWARD DECLARATION */
namespace buffio{
struct readFile;
struct readFilev;
struct writeFile;
struct writeFilev;
struct closeFile;
struct noOp{
  void action(void *no){};
};
};
/* BUFFIO SOCKETOPS FORWARD DECLARATION */
namespace buffio{

};
#endif
