#include "commandlineoption.hpp"

#include <charconv>

namespace cct {
CommandLineOption::CommandLineOption(GroupNameAndPrio optionGroupName, std::string_view fullName, char shortName,
                                     std::string_view valueDescription, std::string_view description)
    : _optionGroupName(optionGroupName.first),
      _fullName(fullName),
      _valueDescription(valueDescription),
      _description(description),
      _prio(optionGroupName.second),
      _shortName(shortName) {}

CommandLineOption::Duration CommandLineOption::ParseDuration(std::string_view durationStr) {
  if (durationStr.find('.') != std::string_view::npos) {
    throw InvalidArgumentException("Time amount should be an integral value");
  }
  std::size_t endAmountPos = durationStr.find_first_of("hmnsu ");
  std::size_t startTimeUnit = durationStr.find_first_of("hmnsu");
  constexpr char kInvalidTimeDurationUnitMsg[] =
      "Cannot parse time duration. Accepted time units are 'h (hours), min (minutes), s (seconds), ms "
      "(milliseconds), us "
      "(microseconds) and ns "
      "(nanoseconds)'";
  if (endAmountPos == std::string_view::npos || startTimeUnit == std::string_view::npos) {
    throw InvalidArgumentException(kInvalidTimeDurationUnitMsg);
  }
  std::string_view timeAmountStr(durationStr.begin(), durationStr.begin() + endAmountPos);
  int64_t timeAmount{};
  std::from_chars(timeAmountStr.data(), timeAmountStr.data() + timeAmountStr.size(), timeAmount);
  std::string_view timeUnitStr(durationStr.begin() + startTimeUnit, durationStr.end());
  if (timeUnitStr == "h") {
    return std::chrono::hours(timeAmount);
  } else if (timeUnitStr == "min") {
    return std::chrono::minutes(timeAmount);
  } else if (timeUnitStr == "s") {
    return std::chrono::seconds(timeAmount);
  } else if (timeUnitStr == "ms") {
    return std::chrono::milliseconds(timeAmount);
  } else if (timeUnitStr == "us") {
    return std::chrono::microseconds(timeAmount);
  } else if (timeUnitStr == "ns") {
    return std::chrono::nanoseconds(timeAmount);
  } else {
    throw InvalidArgumentException(kInvalidTimeDurationUnitMsg);
  }
}

bool CommandLineOption::matches(std::string_view optName) const {
  if (optName.size() == 2 && optName.front() == '-' && optName.back() == _shortName) {
    return true;
  }
  return optName == _fullName;
}

std::string CommandLineOption::shortName() const {
  std::string ret;
  if (_shortName != '\0') {
    ret.push_back('-');
    ret.push_back(_shortName);
  }
  return ret;
}

bool CommandLineOption::operator<(const CommandLineOption& o) const {
  if (_prio != o._prio) {
    return _prio < o._prio;
  }
  if (_optionGroupName != o._optionGroupName) {
    return _optionGroupName < o._optionGroupName;
  }
  return _fullName < o._fullName;
}

}  // namespace cct