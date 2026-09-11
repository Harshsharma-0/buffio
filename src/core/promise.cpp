#include "buffio/core.hpp"
#include "buffio/worker.hpp"
#include <iostream>


namespace buffio{

TaskFinalSuspendAwaitable core::Promise::final_suspend() noexcept{
    if (state.waiter_available)
      state.worker->push(state.waiter);

   return {!state.waiter_available};
  };
};
