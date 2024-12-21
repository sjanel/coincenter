#include "timestring.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "stringconv.hpp"
#include "strnlen.hpp"
#include "timedef.hpp"

namespace cct {

string TimeToString(TimePoint timePoint, const char* format) {
  const std::time_t time = Clock::to_time_t(timePoint);
  std::tm utc{};
#ifdef _WIN32
  if (gmtime_s(&utc, &time)) {
    throw exception("Issue in gmtime_s");
  }
  const std::tm* pUtc = &utc;
#else
  const std::tm* pUtc = gmtime_r(&time, &utc);
  if (pUtc == nullptr) {
    throw exception("Issue in gmtime_r");
  }
#endif
  static constexpr string::size_type kMaxFormatLen = 256;
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

TimePoint StringToTime(std::string_view timeStr, const char* format) {
  std::tm utc{};
  std::istringstream ss{std::string(timeStr)};
  ss >> std::get_time(&utc, format);
  if (ss.fail()) {
    throw exception("Failed to parse time string {}", timeStr);
  }
  // Convert timestamp to epoch time assuming UTC
  std::time_t timet = timegm(&utc);
  return Clock::from_time_t(timet);
}

Nonce Nonce_TimeSinceEpochInMs(Duration delay) {
  const auto nowTime = Clock::now();
  return IntegralToString(TimestampToMillisecondsSinceEpoch(nowTime + delay));
}

}  // namespace cct
