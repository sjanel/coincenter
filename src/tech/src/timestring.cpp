#include "timestring.hpp"

#include <time.h>

#include <chrono>
#include <ctime>

#include "cct_exception.hpp"
#include "stringhelpers.hpp"

namespace cct {

string ToString(std::chrono::system_clock::time_point p, const char* format) {
  const std::time_t t = std::chrono::system_clock::to_time_t(p);
  std::tm utc{};
#ifdef _WIN32
  errno_t err = gmtime_s(&utc, &t);
  if (err) {
    throw exception("Issue in gmtime_s");
  }
  const std::tm* pUtc = &utc;
#else
  const std::tm* pUtc = gmtime_r(&t, &utc);
  if (!pUtc) {
    throw exception("Issue in gmtime_r");
  }
#endif
  string buf(50, '\0');
  std::size_t bytesWritten = std::strftime(buf.data(), buf.size(), format, pUtc);
  if (bytesWritten == 0) {
    throw exception("Buffer size is not sufficient for std::strftime");
  }
  buf.resize(bytesWritten);
  return buf;
}

Nonce Nonce_TimeSinceEpochInMs(int64_t msDelay) {
  const auto n = std::chrono::system_clock::now();
  // The return type of 'count()' is platform dependent. Let's cast it to 'int64_t' which is enough to hold a number of
  // milliseconds from epoch
  int64_t msSinceEpoch =
      static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(n.time_since_epoch()).count());
  return ToString<Nonce>(msSinceEpoch + msDelay);
}

}  // namespace cct
