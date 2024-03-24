#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include "cct_hash.hpp"
#include "timedef.hpp"

namespace cct {

class CommandHeader {
 public:
  constexpr CommandHeader() noexcept = default;

  constexpr CommandHeader(std::string_view groupName, int prio) : _prio(prio), _groupName(groupName) {}

  constexpr std::string_view groupName() const { return _groupName; }

  constexpr int prio() const { return _prio; }

  constexpr std::strong_ordering operator<=>(const CommandHeader&) const = default;

 private:
  // Order of members is important because of the default spaceship operator. _prio should be first
  int _prio = 0;
  std::string_view _groupName;
};

/// Description of a command line option.
class CommandLineOption {
 public:
  constexpr CommandLineOption() noexcept = default;

  constexpr CommandLineOption(CommandHeader commandHeader, std::string_view fullName, char shortName,
                              std::string_view valueDescription, std::string_view description)
      : _commandHeader(commandHeader),
        _fullName(fullName),
        _valueDescription(valueDescription),
        _description(description),
        _shortName(shortName) {}

  constexpr CommandLineOption(CommandHeader commandHeader, std::string_view fullName, std::string_view valueDescription,
                              std::string_view description)
      : CommandLineOption(commandHeader, fullName, '\0', valueDescription, description) {}

  constexpr bool matches(std::string_view optName) const;

  constexpr const CommandHeader& commandHeader() const { return _commandHeader; }
  constexpr std::string_view fullName() const { return _fullName; }
  constexpr std::string_view valueDescription() const { return _valueDescription; }
  constexpr std::string_view description() const { return _description; }

  constexpr char shortNameChar() const { return _shortName; }

  constexpr bool hasShortName() const { return _shortName != '\0'; }

  constexpr std::strong_ordering operator<=>(const CommandLineOption&) const = default;

 private:
  static constexpr std::string_view kLegacyFullNamePrefixOption = "--";

  CommandHeader _commandHeader;
  std::string_view _fullName;
  std::string_view _valueDescription;
  std::string_view _description;
  char _shortName = '\0';
};

/// Basically an extension of the std::optional class with an additional state.
/// Indeed, we want to distinguish the presence of the option with its optional value.
class CommandLineOptionalInt32 {
 public:
  enum class State : int8_t { kValueIsSet, kOptionPresent, kOptionNotPresent };

  constexpr CommandLineOptionalInt32() noexcept = default;

  constexpr CommandLineOptionalInt32(State state) : _state(state) {}

  constexpr CommandLineOptionalInt32(int32_t value) : _value(value), _state(State::kValueIsSet) {}

  constexpr auto& operator*() { return _value; }
  constexpr auto operator*() const { return _value; }

  constexpr bool isPresent() const { return _state == State::kOptionPresent || _state == State::kValueIsSet; }
  constexpr bool isSet() const { return _state == State::kValueIsSet; }

  bool operator==(const CommandLineOptionalInt32&) const noexcept = default;

 private:
  int32_t _value = 0;
  State _state = State::kOptionNotPresent;
};

template <class OptValueType>
struct AllowedCommandLineOptionsBase {
  using CommandLineOptionType =
      std::variant<std::string_view OptValueType::*, std::optional<std::string_view> OptValueType::*,
                   int OptValueType::*, CommandLineOptionalInt32 OptValueType::*, bool OptValueType::*,
                   Duration OptValueType::*>;
  using CommandLineOptionWithValue = std::pair<CommandLineOption, CommandLineOptionType>;
};

constexpr bool CommandLineOption::matches(std::string_view optName) const {
  if (optName.size() == 2 && optName.front() == '-' && optName.back() == _shortName) {
    return true;  // it is a short hand flag
  }
  if (optName == _fullName) {
    return true;  // standard full match
  }
  if (optName.starts_with(kLegacyFullNamePrefixOption)) {
    // backwards compatibility check
    optName.remove_prefix(kLegacyFullNamePrefixOption.length());
    return optName == _fullName;
  }
  return false;
}

}  // namespace cct

// Specialize std::hash<CommandLineOption> for easy usage of CommandLineOption as unordered_map key
namespace std {
template <>
struct hash<cct::CommandHeader> {
  auto operator()(const cct::CommandHeader& commandHeader) const {
    return cct::HashCombine(hash<std::string_view>()(commandHeader.groupName()),
                            static_cast<size_t>(commandHeader.prio()));
  }
};

template <>
struct hash<cct::CommandLineOption> {
  auto operator()(const cct::CommandLineOption& commandLineOption) const {
    return cct::HashCombine(hash<cct::CommandHeader>()(commandLineOption.commandHeader()),
                            hash<std::string_view>()(commandLineOption.fullName()));
  }
};
}  // namespace std