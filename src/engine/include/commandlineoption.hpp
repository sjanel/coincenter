#pragma once

#include <compare>
#include <stdexcept>
#include <string_view>

#include "cct_cctype.hpp"
#include "stringhelpers.hpp"
#include "timehelpers.hpp"

namespace cct {
using InvalidArgumentException = std::invalid_argument;

/// Description of a command line option.
class CommandLineOption {
 public:
  using GroupNameAndPrio = std::pair<std::string_view, int>;

  constexpr CommandLineOption(GroupNameAndPrio optionGroupName, std::string_view fullName, char shortName,
                              std::string_view valueDescription, std::string_view description);

  constexpr CommandLineOption(GroupNameAndPrio optionGroupName, std::string_view fullName,
                              std::string_view valueDescription, std::string_view description)
      : CommandLineOption(optionGroupName, fullName, '\0', valueDescription, description) {}

  constexpr static Duration ParseDuration(std::string_view durationStr);

  constexpr bool matches(std::string_view optName) const;

  constexpr std::string_view optionGroupName() const { return _optionGroupName; }
  constexpr std::string_view fullName() const { return _fullName; }
  constexpr std::string_view description() const { return _description; }
  constexpr std::string_view valueDescription() const { return _valueDescription; }

  constexpr char shortNameChar() const { return _shortName; }

  constexpr bool hasShortName() const { return _shortName != '\0'; }

  constexpr std::strong_ordering operator<=>(const CommandLineOption& o) const;

 private:
  std::string_view _optionGroupName;
  std::string_view _fullName;
  std::string_view _valueDescription;
  std::string_view _description;
  int _prio;
  char _shortName;
};

constexpr CommandLineOption::CommandLineOption(GroupNameAndPrio optionGroupName, std::string_view fullName,
                                               char shortName, std::string_view valueDescription,
                                               std::string_view description)
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

constexpr Duration ToDuration(int64_t timeAmount, std::string_view timeUnitStr) {
  if (timeUnitStr == "y") return std::chrono::years(timeAmount);
  if (timeUnitStr == "mon") return std::chrono::months(timeAmount);
  if (timeUnitStr == "w") return std::chrono::weeks(timeAmount);
  if (timeUnitStr == "d") return std::chrono::days(timeAmount);
  if (timeUnitStr == "h") return std::chrono::hours(timeAmount);
  if (timeUnitStr == "min") return std::chrono::minutes(timeAmount);
  if (timeUnitStr == "s") return std::chrono::seconds(timeAmount);
  if (timeUnitStr == "ms") return std::chrono::milliseconds(timeAmount);
  if (timeUnitStr == "us") return std::chrono::microseconds(timeAmount);
  throw InvalidArgumentException(kInvalidTimeDurationUnitMsg);
}
}  // namespace

constexpr Duration CommandLineOption::ParseDuration(std::string_view durationStr) {
  if (durationStr.find('.') != std::string_view::npos) {
    throw InvalidArgumentException("Time amount should be an integral value");
  }

  const std::size_t s = durationStr.size();
  Duration ret{};
  for (std::size_t p = 0; p < s;) {
    std::size_t intFirst = p;

    while (p < s && isdigit(durationStr[p])) {
      ++p;
    }
    if (intFirst == p) {
      throw InvalidArgumentException(kInvalidTimeDurationUnitMsg);
    }
    std::string_view timeAmountStr(durationStr.begin() + intFirst, durationStr.begin() + p);
    int64_t timeAmount = FromString<int64_t>(timeAmountStr);

    while (p < s && isspace(durationStr[p])) {
      ++p;
    }
    std::size_t unitFirst = p;
    while (p < s && islower(durationStr[p])) {
      ++p;
    }
    if (unitFirst == p) {
      throw InvalidArgumentException(kInvalidTimeDurationUnitMsg);
    }
    std::string_view timeUnitStr(durationStr.begin() + unitFirst, durationStr.begin() + p);
    ret += ToDuration(timeAmount, timeUnitStr);
    while (p < s && isspace(durationStr[p])) {
      ++p;
    }
  }

  if (ret == Duration()) {
    throw InvalidArgumentException(kInvalidTimeDurationUnitMsg);
  }

  return ret;
}

constexpr bool CommandLineOption::matches(std::string_view optName) const {
  if (optName.size() == 2 && optName.front() == '-' && optName.back() == _shortName) {
    return true;
  }
  return optName == _fullName;
}

constexpr std::strong_ordering CommandLineOption::operator<=>(const CommandLineOption& o) const {
  if (_prio != o._prio) {
    return _prio <=> o._prio;
  }
  if (_optionGroupName != o._optionGroupName) {
    return _optionGroupName <=> o._optionGroupName;
  }
  return _fullName <=> o._fullName;
}

}  // namespace cct
