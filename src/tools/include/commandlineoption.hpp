#pragma once

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>

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

  const std::string& optionGroupName() const { return _optionGroupName; }
  const std::string& fullName() const { return _fullName; }
  const std::string& description() const { return _description; }
  const std::string& valueDescription() const { return _valueDescription; }

  std::string shortName() const;

  bool operator<(const CommandLineOption& o) const;

 private:
  std::string _optionGroupName;
  std::string _fullName;
  std::string _valueDescription;
  std::string _description;
  int _prio;
  char _shortName;
};

}  // namespace cct
