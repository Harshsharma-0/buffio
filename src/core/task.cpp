#include "buffio/task.hpp"
#include "buffio/worker.hpp"


namespace buffio{
namespace core{
bool task::core_schedule(buffio::instance& _instance,buffio::vTask task){
 return _instance.push(task);
}

bool task::core_promise_and_push(promise_packed &promise,
                   buffio::vTask task,buffio::vTask self){
  buffio::instance *_instance = buffio::task<char>::from_address(task.address())
             .promise().storage.instance;
  promise = {_instance,task,true};

 return _instance->push(self);
};

}
};


