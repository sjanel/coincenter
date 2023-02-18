#include "timestring.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "cct_exception.hpp"
#include "stringhelpers.hpp"

namespace cct {

string ToString(TimePoint timePoint, const char* format) {
  const std::time_t time = Clock::to_time_t(timePoint);
  std::tm utc{};
#ifdef CCT_MSVC
  errno_t err = gmtime_s(&utc, &time);
  if (err) {
    throw exception("Issue in gmtime_s");
  }
  const std::tm* pUtc = &utc;
#else
  const std::tm* pUtc = gmtime_r(&time, &utc);
  if (pUtc == nullptr) {
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

TimePoint FromString(std::string_view timeStr, const char* format) {
  std::tm utc{};
  std::istringstream ss{std::string(timeStr)};
  ss >> std::get_time(&utc, format);
  return Clock::from_time_t(std::mktime(&utc));  // TODO: fix issue of local time switch
}

Nonce Nonce_TimeSinceEpochInMs(int64_t msDelay) {
  const auto nowTime = Clock::now();
  // The return type of 'count()' is platform dependent. Let's cast it to 'int64_t' which is enough to hold a number of
  // milliseconds from epoch
  int64_t msSinceEpoch =
      static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(nowTime.time_since_epoch()).count());
  return ToString(msSinceEpoch + msDelay);
}

}  // namespace cct
