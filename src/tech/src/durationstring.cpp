#include "durationstring.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"

namespace cct {

namespace {
constexpr std::pair<std::string_view, Duration> kDurationUnits[] = {
    {"y", std::chrono::years(1)},   {"mon", std::chrono::months(1)},      {"w", std::chrono::weeks(1)},
    {"d", std::chrono::days(1)},    {"h", std::chrono::hours(1)},         {"min", std::chrono::minutes(1)},
    {"s", std::chrono::seconds(1)}, {"ms", std::chrono::milliseconds(1)}, {"us", std::chrono::microseconds(1)},
};
}

Duration ParseDuration(std::string_view durationStr) {
  while (!durationStr.empty() && isspace(durationStr.front())) {
    durationStr.remove_prefix(1);
  }

  if (durationStr.empty()) {
    throw invalid_argument("Empty duration is not allowed");
  }
  if (durationStr.find('.') != std::string_view::npos) {
    throw invalid_argument("Time amount should be an integral value");
  }

  static constexpr char kInvalidTimeDurationUnitMsg[] =
      "Cannot parse time duration. Accepted time units are 'y (years), mon (months), w (weeks), d (days), h (hours), "
      "min (minutes), s (seconds), ms (milliseconds) and us (microseconds)'";

  const auto sz = durationStr.size();
  Duration ret{};
  for (std::remove_const_t<decltype(sz)> charPos = 0; charPos < sz;) {
    const auto intFirst = charPos;

    while (charPos < sz && isdigit(durationStr[charPos])) {
      ++charPos;
    }
    if (intFirst == charPos) {
      throw invalid_argument(kInvalidTimeDurationUnitMsg);
    }
    const std::string_view timeAmountStr(durationStr.begin() + intFirst, durationStr.begin() + charPos);
    const int64_t timeAmount = FromString<int64_t>(timeAmountStr);

    while (charPos < sz && isspace(durationStr[charPos])) {
      ++charPos;
    }
    const auto unitFirst = charPos;
    while (charPos < sz && islower(durationStr[charPos])) {
      ++charPos;
    }
    const std::string_view timeUnitStr(durationStr.begin() + unitFirst, durationStr.begin() + charPos);
    const auto it = std::ranges::find_if(kDurationUnits, [timeUnitStr](const auto &durationUnitWithDuration) {
      return durationUnitWithDuration.first == timeUnitStr;
    });
    if (it == std::end(kDurationUnits)) {
      throw invalid_argument(kInvalidTimeDurationUnitMsg);
    }
    ret += timeAmount * it->second;
    while (charPos < sz && isspace(durationStr[charPos])) {
      ++charPos;
    }
  }

  return ret;
}

template <class TimeUnitT>
void AdjustWithUnit(std::string_view unitStr, Duration &dur, string &ret) {
  if (dur >= TimeUnitT(1)) {
    const auto nU = std::chrono::duration_cast<TimeUnitT>(dur).count();

    AppendString(ret, nU);
    ret += unitStr;
    dur -= TimeUnitT(nU);
  }
}

string DurationToString(Duration dur) {
  string ret;

  if (dur == kUndefinedDuration) {
    ret.append("<undef>");
  } else {
    AdjustWithUnit<std::chrono::years>("y", dur, ret);
    AdjustWithUnit<std::chrono::months>("mon", dur, ret);
    AdjustWithUnit<std::chrono::weeks>("w", dur, ret);
    AdjustWithUnit<std::chrono::days>("d", dur, ret);
    AdjustWithUnit<std::chrono::hours>("h", dur, ret);
    AdjustWithUnit<std::chrono::minutes>("min", dur, ret);
    AdjustWithUnit<seconds>("s", dur, ret);
    AdjustWithUnit<milliseconds>("ms", dur, ret);
    AdjustWithUnit<microseconds>("us", dur, ret);
  }

  return ret;
}

}  // namespace cct
