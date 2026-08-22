#ifndef BUFFIO_OP_TABLE
#define BUFFIO_OP_TABLE
#include "buffio/config.hpp"
#include "buffio/file.hpp"

/* ADD FIELDS BEFORE noOp */
namespace buffio{

inline int dispatch_op(op_vec &op,void *data){

    switch(op.index()){
      case 0:
         return -1;
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
      case 5:
        std::get<5>(op)->action(data);
      default:
        return 1;
      break;
    };
    return 0;

};

};
using BGLOBOPVEC = buffio::op_vec; 



#endif
