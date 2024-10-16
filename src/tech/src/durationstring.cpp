#include "durationstring.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <span>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "stringconv.hpp"
#include "timedef.hpp"

namespace cct {

namespace {
using UnitDuration = std::pair<std::string_view, Duration>;

constexpr UnitDuration kDurationUnits[] = {
    {"y", std::chrono::years(1)},   {"mon", std::chrono::months(1)},      {"w", std::chrono::weeks(1)},
    {"d", std::chrono::days(1)},    {"h", std::chrono::hours(1)},         {"min", std::chrono::minutes(1)},
    {"s", std::chrono::seconds(1)}, {"ms", std::chrono::milliseconds(1)}, {"us", std::chrono::microseconds(1)},
};

constexpr char kInvalidTimeDurationUnitMsg[] =
    "Cannot parse time duration. Accepted time units are 'y (years), mon (months), w (weeks), d (days), h (hours), "
    "min (minutes), s (seconds), ms (milliseconds) and us (microseconds)'";

}  // namespace

std::string_view::size_type DurationLen(std::string_view str) {
  std::string_view::size_type ret{};

  while (!str.empty()) {
    const auto sz = str.size();

    std::string_view::size_type charPos{};
    while (charPos < sz && isspace(str[charPos])) {
      ++charPos;
    }

    int value{};
    const auto [ptr, err] = std::from_chars(str.data() + charPos, str.data() + sz, value);
    if (err != std::errc() || value <= 0) {
      break;
    }

    charPos = ptr - str.data();

    while (charPos < sz && isspace(str[charPos])) {
      ++charPos;
    }
    const auto first = charPos;
    while (charPos < sz && islower(str[charPos])) {
      ++charPos;
    }
    const std::string_view timeUnitStr(str.begin() + first, str.begin() + charPos);

    const auto it = std::ranges::find_if(kDurationUnits, [timeUnitStr](const auto &durationUnitWithDuration) {
      return durationUnitWithDuration.first == timeUnitStr;
    });
    if (it == std::end(kDurationUnits)) {
      break;
    }

    // There is a substring with size 'charPos' that represents a duration
    ret += charPos;
    str.remove_prefix(charPos);
  }

  return ret;
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
    const int64_t timeAmount = StringToIntegral<int64_t>(timeAmountStr);

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

namespace {

bool AdjustWithUnit(UnitDuration unitDuration, Duration &dur, int &nbSignificantUnits, string &ret) {
  if (dur >= unitDuration.second) {
    const auto countInThisDurationUnit =
        std::chrono::duration_cast<decltype(unitDuration.second)>(dur).count() / unitDuration.second.count();
    AppendIntegralToString(ret, countInThisDurationUnit);

    ret += unitDuration.first;
    dur -= countInThisDurationUnit * unitDuration.second;
    if (--nbSignificantUnits == 0) {
      return true;
    }
  }
  return false;
}

bool AdjustWithUnit(UnitDuration unitDuration, Duration &dur, int &nbSignificantUnits, std::span<char> &ret) {
  if (dur >= unitDuration.second) {
    const auto countInThisDurationUnit =
        std::chrono::duration_cast<decltype(unitDuration.second)>(dur).count() / unitDuration.second.count();
    auto actualBuf = IntegralToCharBuffer(ret, countInThisDurationUnit);
    ret = ret.subspan(actualBuf.size());

    if (ret.size() < unitDuration.first.size()) {
      throw invalid_argument("Buffer is too small to store unit");
    }

    std::ranges::copy(unitDuration.first, ret.begin());
    ret = ret.subspan(unitDuration.first.size());

    dur -= countInThisDurationUnit * unitDuration.second;
    if (--nbSignificantUnits == 0) {
      return true;
    }
  }
  return false;
}

constexpr std::string_view kUndefStr = "<undef>";

}  // namespace

string DurationToString(Duration dur, int nbSignificantUnits) {
  string ret;

  if (dur == kUndefinedDuration) {
    ret.append(kUndefStr);
  } else {
    std::ranges::find_if(kDurationUnits, [&dur, &nbSignificantUnits, &ret](const auto &unitDuration) {
      return AdjustWithUnit(unitDuration, dur, nbSignificantUnits, ret);
    });
  }

  return ret;
}

std::span<char> DurationToBuffer(Duration dur, std::span<char> buffer, int nbSignificantUnits) {
  if (dur == kUndefinedDuration) {
    if (buffer.size() < kUndefStr.size()) {
      throw invalid_argument("Buffer is too small to store '<undef>'");
    }
    auto inOutRes = std::ranges::copy(kUndefStr, buffer.begin());
    return {buffer.begin(), inOutRes.out};
  }

  auto begBuf = buffer.data();

  std::ranges::find_if(kDurationUnits, [&dur, &nbSignificantUnits, &buffer](const auto &unitDuration) {
    return AdjustWithUnit(unitDuration, dur, nbSignificantUnits, buffer);
  });

  return {begBuf, buffer.data()};
}

}  // namespace cct
