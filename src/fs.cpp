#include "buffio/fs.hpp"
#include "buffio/task.hpp"
#include "buffio/worker.hpp"


void buffio::AwaitableFileBase::await_suspend(buffio::CoroutineHandle task_){
 buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  op_state.task = task_;
  worker_->push(op_state);

};

void buffio::OpenFileAwaiter::await_suspend(
              buffio::CoroutineHandle task_){

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  op_state.task = task_;
  worker_->push(op_state);

};

