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

int dispatch_op(op_vec op,void *data);
};
using BGLOBOPVEC = buffio::op_vec; 



#endif
