#include "cct_nonce.hpp"

#include <time.h>

#include <chrono>
#include <ctime>

#include "cct_exception.hpp"
#include "stringhelpers.hpp"

namespace cct {

Nonce Nonce_TimeSinceEpoch() {
  const auto p1 = std::chrono::system_clock::now();
  uintmax_t msSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(p1.time_since_epoch()).count();
  return ToString<Nonce>(msSinceEpoch);
}

Nonce Nonce_LiteralDate() {
  std::time_t t = std::time(0);
  std::tm utc{};
#ifdef _WIN32
  errno_t err = gmtime_s(&utc, &t);
  if (err) {
    throw exception("Issue in gmtime_s");
  }
  std::tm* pUtc = &utc;
#else
  std::tm* pUtc = gmtime_r(&t, &utc);
  if (!pUtc) {
    throw exception("Issue in gmtime_r");
  }
#endif
  Nonce buf(20, ' ');
  std::size_t bytesWritten = std::strftime(buf.data(), buf.size(), "%Y-%m-%dT%H:%M:%S", pUtc);
  if (bytesWritten == 0) {
    throw exception("Buffer size is not sufficient for std::strftime");
  }
  buf.resize(bytesWritten);
  return buf;
}

}  // namespace cct
