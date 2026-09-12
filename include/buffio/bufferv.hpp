#ifndef BUFFIO_BUFFERV
#define BUFFIO_BUFFERV

#include "buffio/config.hpp"
#include <utility>

#if defined(BUFFIO_OS_WINDOWS)
struct iovec {
  void *iov_base;
  size_t iov_len;
};
#endif

namespace buffio {
using BufferiovBase = struct iovec;

struct Bufferiov : public BufferiovBase {
  void operator=(char *data) { iov_base = static_cast<void *>(data); };
  void operator=(size_t len) { iov_len = len; };
};

class BuffervState {

public:
  std::pair<Bufferiov *, size_t> get() const { return {io_vecs, size}; };

  bool CreateVec(int num);
  bool MakeEntry(int idx, char *buffer, size_t bufSize);
  BuffervState() = default;

private:
  Bufferiov *io_vecs = nullptr;
  size_t size = 0;
  size_t size_max = 0;
  bool iown = false;
};
}; // namespace buffio
#endif
