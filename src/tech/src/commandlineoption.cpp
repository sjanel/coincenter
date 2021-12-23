#include "commandlineoption.hpp"

#include "stringhelpers.hpp"

namespace cct {
CommandLineOption::CommandLineOption(GroupNameAndPrio optionGroupName, std::string_view fullName, char shortName,
                                     std::string_view valueDescription, std::string_view description)
    : _optionGroupName(optionGroupName.first),
      _fullName(fullName),
      _valueDescription(valueDescription),
      _description(description),
      _prio(optionGroupName.second),
      _shortName(shortName) {}

namespace {
constexpr char kInvalidTimeDurationUnitMsg[] =
    "Cannot parse time duration. Accepted time units are 'y (years), mon (months), w (weeks), d (days), h (hours), "
    "min (minutes), s (seconds), ms (milliseconds), us (microseconds) and ns (nanoseconds)'";

CommandLineOption::Duration ToDuration(int64_t timeAmount, std::string_view timeUnitStr) {
  if (timeUnitStr == "y") {
    return std::chrono::years(timeAmount);
  }
  if (timeUnitStr == "mon") {
    return std::chrono::months(timeAmount);
  }
  if (timeUnitStr == "w") {
    return std::chrono::weeks(timeAmount);
  }
  if (timeUnitStr == "d") {
    return std::chrono::days(timeAmount);
  }
  if (timeUnitStr == "h") {
    return std::chrono::hours(timeAmount);
  }
  if (timeUnitStr == "min") {
    return std::chrono::minutes(timeAmount);
  }
  if (timeUnitStr == "s") {
    return std::chrono::seconds(timeAmount);
  }
  if (timeUnitStr == "ms") {
    return std::chrono::milliseconds(timeAmount);
  }
  if (timeUnitStr == "us") {
    return std::chrono::microseconds(timeAmount);
  }
  if (timeUnitStr == "ns") {
    return std::chrono::nanoseconds(timeAmount);
  }
  throw InvalidArgumentException(kInvalidTimeDurationUnitMsg);
}
}  // namespace

CommandLineOption::Duration CommandLineOption::ParseDuration(std::string_view durationStr) {
  if (durationStr.find('.') != std::string_view::npos) {
    throw InvalidArgumentException("Time amount should be an integral value");
  }

  const std::size_t s = durationStr.size();
  static constexpr Duration kZeroDuration = std::chrono::seconds(0);
  Duration ret{kZeroDuration};
  for (std::size_t p = 0; p < s;) {
    std::size_t intFirst = p;

    while (p < s && durationStr[p] >= '0' && durationStr[p] <= '9') {
      ++p;
    }
    if (intFirst == p) {
      throw InvalidArgumentException(kInvalidTimeDurationUnitMsg);
    }
    std::string_view timeAmountStr(durationStr.begin() + intFirst, durationStr.begin() + p);
    int64_t timeAmount = FromString<int64_t>(timeAmountStr);

    while (p < s && durationStr[p] == ' ') {
      ++p;
    }
    std::size_t unitFirst = p;
    while (p < s && durationStr[p] >= 'a' && durationStr[p] <= 'z') {
      ++p;
    }
    if (unitFirst == p) {
      throw InvalidArgumentException(kInvalidTimeDurationUnitMsg);
    }
    std::string_view timeUnitStr(durationStr.begin() + unitFirst, durationStr.begin() + p);
    ret += ToDuration(timeAmount, timeUnitStr);
    while (p < s && durationStr[p] == ' ') {
      ++p;
    }
  }

  if (ret == kZeroDuration) {
    throw InvalidArgumentException(kInvalidTimeDurationUnitMsg);
  }

  return ret;
}

bool CommandLineOption::matches(std::string_view optName) const {
  if (optName.size() == 2 && optName.front() == '-' && optName.back() == _shortName) {
    return true;
  }
  return optName == _fullName;
}

string CommandLineOption::shortName() const {
  string ret;
  if (_shortName != '\0') {
    ret.push_back('-');
    ret.push_back(_shortName);
  }
  return ret;
}

std::strong_ordering CommandLineOption::operator<=>(const CommandLineOption& o) const {
  if (_prio != o._prio) {
    return _prio <=> o._prio;
  }
  if (_optionGroupName != o._optionGroupName) {
    return _optionGroupName <=> o._optionGroupName;
  }
  return _fullName <=> o._fullName;
}

}  // namespace cct