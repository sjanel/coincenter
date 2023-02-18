#include "durationstring.hpp"

#include <cstdint>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "stringhelpers.hpp"

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

string DurationToString(Duration d) {
  string ret;
  if (d >= std::chrono::years(1)) {
    int64_t nbY = std::chrono::duration_cast<std::chrono::years>(d).count();
    AppendString(ret, nbY);
    ret.push_back('y');
    d -= std::chrono::years(nbY);
  }
  if (d >= std::chrono::months(1)) {
    int64_t nM = std::chrono::duration_cast<std::chrono::months>(d).count();
    AppendString(ret, nM);
    ret.append("mon");
    d -= std::chrono::months(nM);
  }
  if (d >= std::chrono::weeks(1)) {
    int64_t nbW = std::chrono::duration_cast<std::chrono::weeks>(d).count();
    AppendString(ret, nbW);
    ret.push_back('w');
    d -= std::chrono::weeks(nbW);
  }
  if (d >= std::chrono::days(1)) {
    int64_t nbD = std::chrono::duration_cast<std::chrono::days>(d).count();
    AppendString(ret, nbD);
    ret.push_back('d');
    d -= std::chrono::days(nbD);
  }
  if (d >= std::chrono::hours(1)) {
    int64_t nbH = std::chrono::duration_cast<std::chrono::hours>(d).count();
    AppendString(ret, nbH);
    ret.push_back('h');
    d -= std::chrono::hours(nbH);
  }
  if (d >= std::chrono::minutes(1)) {
    int64_t nbMin = std::chrono::duration_cast<std::chrono::minutes>(d).count();
    AppendString(ret, nbMin);
    ret.append("min");
    d -= std::chrono::minutes(nbMin);
  }
  if (d >= std::chrono::seconds(1)) {
    int64_t nbS = std::chrono::duration_cast<std::chrono::seconds>(d).count();
    AppendString(ret, nbS);
    ret.push_back('s');
    d -= std::chrono::seconds(nbS);
  }
  if (d >= std::chrono::milliseconds(1)) {
    int64_t nbMs = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    AppendString(ret, nbMs);
    ret.append("ms");
    d -= std::chrono::milliseconds(nbMs);
  }
  if (d >= std::chrono::microseconds(1)) {
    int64_t nbUs = std::chrono::duration_cast<std::chrono::microseconds>(d).count();
    AppendString(ret, nbUs);
    ret.append("us");
  }

  return ret;
}

}  // namespace cct