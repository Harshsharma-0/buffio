#ifndef BUFFIO_OP_TABLE
#define BUFFIO_OP_TABLE
#include "buffio/config.hpp"
#include "buffio/core.hpp"
#include <variant>


/* ADD FIELDS BEFORE noOp */
namespace buffio{
using op_vec = std::variant<
                 buffio::readFile*,
                 buffio::readFilev*,
                 buffio::writeFile*,
                 buffio::writeFilev*,
                 buffio::noOp *
                 >;

};
using BGLOBOPVEC = buffio::op_vec; 



#define BUFFIO_OP_ACTION_TABLE(varientName,whichIDX,_onDefault) \
    switch(whichIDX){                                                       \
      case 0: buffio::readFile::action(std::get<0>(varientName));   break;  \
      case 1: buffio::readFilev::action(std::get<1>(varientName));  break;  \
      case 2: buffio::writeFile::action(std::get<2>(varientName));  break;  \
      case 3: buffio::writeFilev::action(std::get<3>(varientName)); break;  \
      default: _onDefault                                                   \
    };
#endif
