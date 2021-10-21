#pragma once

#include <chrono>
#include <stdexcept>
#include <string_view>

#include "cct_string.hpp"

namespace cct {
using InvalidArgumentException = std::invalid_argument;

/// Description of a command line option.
class CommandLineOption {
 public:
  using GroupNameAndPrio = std::pair<std::string_view, int>;
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;
  using Duration = Clock::duration;

  CommandLineOption(GroupNameAndPrio optionGroupName, std::string_view fullName, char shortName,
                    std::string_view valueDescription, std::string_view description);

  CommandLineOption(GroupNameAndPrio optionGroupName, std::string_view fullName, std::string_view valueDescription,
                    std::string_view description)
      : CommandLineOption(optionGroupName, fullName, '\0', valueDescription, description) {}

  static Duration ParseDuration(std::string_view durationStr);

  bool matches(std::string_view optName) const;

  std::string_view optionGroupName() const { return _optionGroupName; }
  std::string_view fullName() const { return _fullName; }
  std::string_view description() const { return _description; }
  std::string_view valueDescription() const { return _valueDescription; }

  string shortName() const;

  bool operator<(const CommandLineOption& o) const;

 private:
  string _optionGroupName;
  string _fullName;
  string _valueDescription;
  string _description;
  int _prio;
  char _shortName;
};

}  // namespace cct
