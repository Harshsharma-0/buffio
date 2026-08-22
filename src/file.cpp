#include "buffio/file.hpp"
#include "buffio/worker.hpp"

void buffio::ReadFileAwaiter::await_suspend(buffio::CoroutineHandle task_){ 

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  identifier = this;
  worker = worker_;
  task = task_;
  worker_->push(this->identifier);

};


void buffio::ReadvFileAwaiter::await_suspend(buffio::CoroutineHandle task_){

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  identifier = this;
  worker = worker_;
  task = task_;
  worker_->push(this->identifier);

};

void buffio::WriteFileAwaiter::await_suspend(buffio::CoroutineHandle task_){

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  identifier = this;
  worker = worker_;
  task = task_;
  worker_->push(this->identifier);

};

void buffio::WritevFileAwaiter::await_suspend(buffio::CoroutineHandle task_){

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  identifier = this;
  worker = worker_;
  task = task_;
  worker_->push(this->identifier);


};

