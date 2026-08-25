#include "buffio/fs.hpp"
#include "buffio/worker.hpp"


void buffio::AwaitableFileBase::await_suspend(buffio::CoroutineHandle task_){
 buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  op_state.task = task_;
  worker_->push(op_state);

};

void buffio::AwaitableFilevBase::await_suspend(buffio::CoroutineHandle task_){
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
/*
void buffio::ReadFileAwaiter::await_suspend(buffio::CoroutineHandle task_){ 

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  op_state.task = task_;
  worker_->push(op_state);

};
void buffio::WriteFileAwaiter::await_suspend(buffio::CoroutineHandle task_){

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  op_state.task = task_;

  worker_->push(op_state);

};

void buffio::ReadvFileAwaiter::await_suspend(buffio::CoroutineHandle task_){

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  op_state.task = task_;
  worker_->push(op_state);

};


void buffio::WritevFileAwaiter::await_suspend(buffio::CoroutineHandle task_){

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  op_state.task = task_;
  
  worker_->push(op_state);
};
*/
