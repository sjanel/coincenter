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

template <class TimeUnitT>
void AdjustWithUnit(std::string_view unitStr, Duration &dur, string &ret) {
  if (dur >= TimeUnitT(1)) {
    auto nU = std::chrono::duration_cast<TimeUnitT>(dur).count();
    AppendString(ret, nU);
    ret += unitStr;
    dur -= TimeUnitT(nU);
  }
}

string DurationToString(Duration dur) {
  string ret;

  AdjustWithUnit<std::chrono::years>("y", dur, ret);
  AdjustWithUnit<std::chrono::months>("mon", dur, ret);
  AdjustWithUnit<std::chrono::weeks>("w", dur, ret);
  AdjustWithUnit<std::chrono::days>("d", dur, ret);
  AdjustWithUnit<std::chrono::hours>("h", dur, ret);
  AdjustWithUnit<std::chrono::minutes>("min", dur, ret);
  AdjustWithUnit<std::chrono::seconds>("s", dur, ret);
  AdjustWithUnit<std::chrono::milliseconds>("ms", dur, ret);
  AdjustWithUnit<std::chrono::microseconds>("us", dur, ret);

  return ret;
}

}  // namespace cct
