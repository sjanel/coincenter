#include "timestring.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include "cct_exception.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "ipow.hpp"
#include "simple-charconv.hpp"
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
#ifdef _WIN32
  std::time_t timet = _mkgmtime(&utc);
#else
  std::time_t timet = timegm(&utc);
#endif
  return Clock::from_time_t(timet);
}

Nonce Nonce_TimeSinceEpochInMs(Duration delay) {
  return IntegralToString(TimestampToMillisecondsSinceEpoch(Clock::now() + delay));
}

char* TimeToStringIso8601UTC(TimePoint timePoint, char* buffer) {
  const auto days = std::chrono::floor<std::chrono::days>(timePoint);
  const std::chrono::year_month_day ymd{days};

  // Format: 'YYYY-MM-DDTHH:MM:SSZ'
  write4(buffer, static_cast<int32_t>(ymd.year()));
  buffer[4] = '-';
  write2(buffer + 5, static_cast<unsigned int>(ymd.month()));
  buffer[7] = '-';
  write2(buffer + 8, static_cast<unsigned int>(ymd.day()));

  const std::chrono::hh_mm_ss hhMmSs{std::chrono::floor<seconds>(timePoint - days)};

  buffer[10] = 'T';
  write2(buffer + 11, static_cast<uint8_t>(hhMmSs.hours().count()));
  buffer[13] = ':';
  write2(buffer + 14, static_cast<uint8_t>(hhMmSs.minutes().count()));
  buffer[16] = ':';
  write2(buffer + 17, static_cast<uint8_t>(hhMmSs.seconds().count()));
  buffer[19] = 'Z';
  return buffer + 20;  // Return a pointer after the last char written
}

char* TimeToStringIso8601UTCWithMillis(TimePoint timePoint, char* buffer) {
  const auto days = std::chrono::floor<std::chrono::days>(timePoint);
  const std::chrono::year_month_day ymd{days};

  // Format: 'YYYY-MM-DDTHH:MM:SS.sssZ'
  write4(buffer, static_cast<int32_t>(ymd.year()));
  buffer[4] = '-';
  write2(buffer + 5, static_cast<unsigned int>(ymd.month()));
  buffer[7] = '-';
  write2(buffer + 8, static_cast<unsigned int>(ymd.day()));

  const std::chrono::hh_mm_ss hhMmSs{std::chrono::floor<milliseconds>(timePoint - days)};

  buffer[10] = 'T';
  write2(buffer + 11, static_cast<uint8_t>(hhMmSs.hours().count()));
  buffer[13] = ':';
  write2(buffer + 14, static_cast<uint8_t>(hhMmSs.minutes().count()));
  buffer[16] = ':';
  write2(buffer + 17, static_cast<uint8_t>(hhMmSs.seconds().count()));
  buffer[19] = '.';
  write3(buffer + 20, static_cast<uint32_t>(hhMmSs.subseconds().count()));
  buffer[23] = 'Z';
  return buffer + 24;  // Return a pointer after the last char written
}

TimePoint StringToTimeISO8601UTC(const char* begPtr, const char* endPtr) {
  if (CCT_UNLIKELY(endPtr - begPtr < 19)) {
    throw invalid_argument("ISO8601UTC Time string '{}' is too short, expected at least 19 characters",
                           std::string_view(begPtr, endPtr));
  }

  TimePoint ts = std::chrono::sys_days{std::chrono::year_month_day{std::chrono::year(parse4(begPtr)),
                                                                   std::chrono::month(parse2(begPtr + 5)),
                                                                   std::chrono::day(parse2(begPtr + 8))}} +
                 std::chrono::hours{parse2(begPtr + 11)} + std::chrono::minutes{parse2(begPtr + 14)} +
                 std::chrono::seconds{parse2(begPtr + 17)};

  if (begPtr + 20 < endPtr) {
    if (*(endPtr - 1) == 'Z') {
      // remove the Z, consider UTC anyways
      --endPtr;
    }
    // parse sub-seconds parts until the end (possible 'Z' removed)
    auto subSecondsSz = std::min<long>(endPtr - begPtr - 20, 9);
    switch (subSecondsSz) {
      case 3:  // milliseconds
        ts += std::chrono::milliseconds{parse3(begPtr + 20)};
        break;
      case 6:  // microseconds
        ts += std::chrono::microseconds{parse6(begPtr + 20)};
        break;
      case 9:  // nanoseconds
        ts += std::chrono::nanoseconds{parse9(begPtr + 20)};
        break;
      default:
        ts += std::chrono::nanoseconds{parse(begPtr + 20, subSecondsSz) * ipow10(9 - subSecondsSz)};
        break;
    }
  }

  return ts;
}

}  // namespace cct
