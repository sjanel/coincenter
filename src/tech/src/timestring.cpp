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
  static constexpr string::size_type kMaxFormatLen = 4096;
  string::size_type bufSize = strnlen(format, kMaxFormatLen);
  string buf(bufSize, '\0');

  std::size_t bytesWritten;
  do {
    bytesWritten = std::strftime(buf.data(), buf.size(), format, pUtc);
    if (bytesWritten == 0) {
      if (buf.size() > kMaxFormatLen) {
        throw exception("Format string {} is too long, maximum length is {}", format, kMaxFormatLen);
      }
      buf.resize((3U * buf.size()) / 2U);
    }
  } while (bytesWritten == 0);

  buf.resize(bytesWritten);
  return buf;
}

TimePoint FromString(std::string_view timeStr, const char* format) {
  std::tm utc{};
  std::istringstream ss{std::string(timeStr)};
  ss >> std::get_time(&utc, format);
  return Clock::from_time_t(std::mktime(&utc));  // TODO: fix issue of local time switch
}

Nonce Nonce_TimeSinceEpochInMs(Duration delay) {
  const auto nowTime = Clock::now();
  return ToString(std::chrono::duration_cast<std::chrono::milliseconds>(nowTime.time_since_epoch() + delay).count());
}

}  // namespace cct
