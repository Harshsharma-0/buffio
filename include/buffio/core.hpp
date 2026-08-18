#ifndef BUFFIO_CORE
#define BUFFIO_CORE
#include <cassert>
#include <coroutine>
#include <utility>

/* BUFFIO FORWARD DECLARED */
namespace buffio{
class instance;
class instanceCore;
};

/* BUFFIO TASK DECLARTION */
namespace buffio {
template <typename promiseT> class promise;
using vTask = std::coroutine_handle<>;

template <typename promisePackedT> struct promisePacked {
  buffio::instance *instance;
  buffio::vTask waiter;
  bool waiterAvailable; 
  promisePackedT val;
};
struct taskExt {
  buffio::vTask task;
  struct taskExt *next;
};
};

/* BUFFIO CORE */
namespace buffio{
namespace core {
class awaitable {};
class promise {};
class instance {};
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
 static void action(struct noOP *no){};
};
};
/* BUFFIO SOCKETOPS FORWARD DECLARATION */
namespace buffio{

};
#endif
