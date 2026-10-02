#include "buffio/fs.hpp"
#include "buffio/task.hpp"
#include "buffio/worker.hpp"
#include "buffio/opcode.hpp"

void buffio::AwaitableFileBase::await_suspend(buffio::CoroutineHandle task_){
 buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  op_state.task = task_;
  op_state.op_class = buffio::OpClass::FileIo,
  op_state.data = static_cast<void*>(this);
  worker_->push(op_state);
};

void buffio::OpenFileAwaiter::await_suspend(
              buffio::CoroutineHandle task_){

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  op_state.op_class = buffio::OpClass::OpenOp,
  op_state.task = task_;
  op_state.op_code = OpCode::Open;
  op_state.data = static_cast<void *>(this);

  worker_->push(op_state);

};

void buffio::AwaitableFsBase::await_suspend(
              buffio::CoroutineHandle task_){

  buffio::Worker *worker_ = 
       buffio::task<char>::from_address(task_.address())
      .promise().state.worker;

  op_state.op_class = buffio::OpClass::FsOps,
  op_state.task = task_;
  op_state.op_code = OpCode::MkDir;
  op_state.data = static_cast<void *>(this);
  worker_->push(op_state);

};

