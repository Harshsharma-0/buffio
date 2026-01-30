#ifndef BUFFIO_MUX
#define BUFFIO_MUX
/*
 * not for public use
 */

#include "../buffiolfqueue.hpp"
#include <variant>
#include <unordered_map>

template <typename... types> class buffiomux {

public:
private:
  std::unordered_map<int,std::variant<types...>> mux;
};
#endif
