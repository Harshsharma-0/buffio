#include "buffio/optable.hpp"
#include "buffio/file.hpp"


int buffio::dispatch_op(buffio::op_vec op,void *data){
    switch(op.index()){
      case 0:
        std::get<0>(op)->action(data);
       break;
      case 1:
        std::get<1>(op)->action(data);
       break;
      case 2:
        std::get<2>(op)->action(data);
       break;
      case 3:
        std::get<3>(op)->action(data);
      break;
      case 4:
        std::get<4>(op)->action(data);
      break;
      default:
        return 1;
      break;
    };
    return 0;
};

