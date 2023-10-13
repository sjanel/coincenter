#include "durationstring.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"

namespace cct {
Duration ParseDuration(std::string_view durationStr) {
  if (durationStr.empty()) {
    throw invalid_argument("Empty duration is not allowed");
  }
  if (durationStr.find('.') != std::string_view::npos) {
    throw invalid_argument("Time amount should be an integral value");
  }

  static constexpr char kInvalidTimeDurationUnitMsg[] =
      "Cannot parse time duration. Accepted time units are 'y (years), mon (months), w (weeks), d (days), h (hours), "
      "min (minutes), s (seconds), ms (milliseconds) and us (microseconds)'";

  const std::size_t sz = durationStr.size();
  Duration ret{};
  for (std::size_t charPos = 0; charPos < sz;) {
    std::size_t intFirst = charPos;

    while (charPos < sz && isdigit(durationStr[charPos])) {
      ++charPos;
    }
    if (intFirst == charPos) {
      throw invalid_argument(kInvalidTimeDurationUnitMsg);
    }
    std::string_view timeAmountStr(durationStr.begin() + intFirst, durationStr.begin() + charPos);
    int64_t timeAmount = FromString<int64_t>(timeAmountStr);

    while (charPos < sz && isspace(durationStr[charPos])) {
      ++charPos;
    }
    std::size_t unitFirst = charPos;
    while (charPos < sz && islower(durationStr[charPos])) {
      ++charPos;
    }
    std::string_view timeUnitStr(durationStr.begin() + unitFirst, durationStr.begin() + charPos);
    if (timeUnitStr == "y") {
      ret += std::chrono::years(timeAmount);
    } else if (timeUnitStr == "mon") {
      ret += std::chrono::months(timeAmount);
    } else if (timeUnitStr == "w") {
      ret += std::chrono::weeks(timeAmount);
    } else if (timeUnitStr == "d") {
      ret += std::chrono::days(timeAmount);
    } else if (timeUnitStr == "h") {
      ret += std::chrono::hours(timeAmount);
    } else if (timeUnitStr == "min") {
      ret += std::chrono::minutes(timeAmount);
    } else if (timeUnitStr == "s") {
      ret += std::chrono::seconds(timeAmount);
    } else if (timeUnitStr == "ms") {
      ret += std::chrono::milliseconds(timeAmount);
    } else if (timeUnitStr == "us") {
      ret += std::chrono::microseconds(timeAmount);
    } else {
      throw invalid_argument(kInvalidTimeDurationUnitMsg);
    }
    while (charPos < sz && isspace(durationStr[charPos])) {
      ++charPos;
    }
  }

  return ret;
}

string DurationToString(Duration dur) {
  string ret;
  if (dur >= std::chrono::years(1)) {
    int64_t nbY = std::chrono::duration_cast<std::chrono::years>(dur).count();
    AppendString(ret, nbY);
    ret.push_back('y');
    dur -= std::chrono::years(nbY);
  }
  if (dur >= std::chrono::months(1)) {
    int64_t nM = std::chrono::duration_cast<std::chrono::months>(dur).count();
    AppendString(ret, nM);
    ret.append("mon");
    dur -= std::chrono::months(nM);
  }
  if (dur >= std::chrono::weeks(1)) {
    int64_t nbW = std::chrono::duration_cast<std::chrono::weeks>(dur).count();
    AppendString(ret, nbW);
    ret.push_back('w');
    dur -= std::chrono::weeks(nbW);
  }
  if (dur >= std::chrono::days(1)) {
    int64_t nbD = std::chrono::duration_cast<std::chrono::days>(dur).count();
    AppendString(ret, nbD);
    ret.push_back('d');
    dur -= std::chrono::days(nbD);
  }
  if (dur >= std::chrono::hours(1)) {
    int64_t nbH = std::chrono::duration_cast<std::chrono::hours>(dur).count();
    AppendString(ret, nbH);
    ret.push_back('h');
    dur -= std::chrono::hours(nbH);
  }
  if (dur >= std::chrono::minutes(1)) {
    int64_t nbMin = std::chrono::duration_cast<std::chrono::minutes>(dur).count();
    AppendString(ret, nbMin);
    ret.append("min");
    dur -= std::chrono::minutes(nbMin);
  }
  if (dur >= std::chrono::seconds(1)) {
    int64_t nbS = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
    AppendString(ret, nbS);
    ret.push_back('s');
    dur -= std::chrono::seconds(nbS);
  }
  if (dur >= std::chrono::milliseconds(1)) {
    int64_t nbMs = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    AppendString(ret, nbMs);
    ret.append("ms");
    dur -= std::chrono::milliseconds(nbMs);
  }
  if (dur >= std::chrono::microseconds(1)) {
    int64_t nbUs = std::chrono::duration_cast<std::chrono::microseconds>(dur).count();
    AppendString(ret, nbUs);
    ret.append("us");
  }

  return ret;
}

}  // namespace cct
