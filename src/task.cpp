#include "buffio/task.hpp"
#include "buffio/worker.hpp"


namespace buffio{
namespace core{
bool Task::core_schedule(buffio::Worker& worker,
                          buffio::CoroutineHandle task){
 return worker.push(task);
}

bool Task::promise_and_push(PromiseState &promise,
                            buffio::CoroutineHandle task,
                            buffio::CoroutineHandle self){

  buffio::Worker *worker = buffio::task<char>::from_address(task.address())
             .promise().state.worker;
  promise = {worker,task,true};

 return worker->push(self);
};

}
};


