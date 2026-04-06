#pragma once
#include "common.hpp"
#include <cerrno>
#include <unistd.h>
namespace buffio {

class action {

  using xeturn = buffio::promiseHandle;

public:
  static action::xeturn read(buffioHeader *header);
  static action::xeturn write(buffioHeader *header);

  static action::xeturn readFile(buffioHeader *header);
  static action::xeturn writeFile(buffioHeader *header);

  static action::xeturn asyncConnect(buffioHeader *header);
  static action::xeturn waitConnect(buffioHeader *header);

  static action::xeturn waitAccept(buffioHeader *header);

  static action::xeturn asyncRead(buffioHeader *header);
  static action::xeturn asyncWrite(buffioHeader *header);

  static action::xeturn asyncReadFile(buffioHeader *header);
  static action::xeturn asyncWriteFile(buffioHeader *header);

  static action::xeturn asyncAccept(buffioHeader *header);
  static action::xeturn asyncAcceptIpv4(buffioHeader *header);
  static action::xeturn asyncAcceptIpv6(buffioHeader *header);

  static action::xeturn clampThread(buffioHeader *header);

  static action::xeturn propBack(buffioHeader *header);
  
};

}; // namespace buffio
