#pragma once
#include "buffio/common.hpp"


namespace buffio{
class makeFlow{
  public:
          

 makeFlow &make(void *data,runFlow callback){
      auto ref = new flow;
      ref->flowing = callback;
      ref->data = data;
      head = ref;
      ref->current = head;
      next = head;
      return *this;
    }
    makeFlow &then(runFlow callback,void *data){
      auto ref = new flow;
      ref->flowing = callback;
      ref->data = data;
      next->chainNext = ref;
      next = ref;
      return *this;
    };
    void flowDone();
 private:
 flow *head = nullptr;
 flow *next = nullptr;
};
};
