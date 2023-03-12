#pragma once

#include <compare>
#include <optional>
#include <string_view>
#include <variant>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "stringhelpers.hpp"
#include "timedef.hpp"

namespace cct {

/// Description of a command line option.
class CommandLineOption {
 public:
  using GroupNameAndPrio = std::pair<const char*, int>;

  constexpr CommandLineOption() noexcept = default;

  template <class StringViewType>
  constexpr CommandLineOption(GroupNameAndPrio optionGroupName, const char* fullName, char shortName,
                              const char* valueDescription, StringViewType description)
      : _optionGroupName(optionGroupName.first),
        _fullName(fullName),
        _valueDescription(valueDescription),
        _description(description),
        _prio(optionGroupName.second),
        _shortName(shortName) {
    static_assert(std::is_same_v<StringViewType, std::string_view> || std::is_same_v<StringViewType, const char*>);
  }

  template <class StringViewType>
  constexpr CommandLineOption(GroupNameAndPrio optionGroupName, const char* fullName, const char* valueDescription,
                              StringViewType description)
      : CommandLineOption(optionGroupName, fullName, '\0', valueDescription, description) {}

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
  int _prio = 0;
  char _shortName = '\0';
};

/// Basically an extension of the std::optional class with an additional state.
/// Indeed, we want to distinguish the presence of the option with its optional value.
class CommandLineOptionalInt {
 public:
  enum class State : int8_t { kValueIsSet, kOptionPresent, kOptionNotPresent };

  constexpr CommandLineOptionalInt() noexcept = default;

  constexpr CommandLineOptionalInt(State state) : _state(state) {}

  constexpr CommandLineOptionalInt(int value) : _value(value), _state(State::kValueIsSet) {}

  constexpr int& operator*() { return _value; }
  constexpr int operator*() const { return _value; }

  constexpr bool isPresent() const { return _state == State::kOptionPresent || _state == State::kValueIsSet; }
  constexpr bool isSet() const { return _state == State::kValueIsSet; }

 private:
  int _value = 0;
  State _state = State::kOptionNotPresent;
};

template <class OptValueType>
struct AllowedCommandLineOptionsBase {
  using CommandLineOptionType =
      std::variant<std::string_view OptValueType::*, std::optional<std::string_view> OptValueType::*,
                   int OptValueType::*, CommandLineOptionalInt OptValueType::*, bool OptValueType::*,
                   Duration OptValueType::*>;
  using CommandLineOptionWithValue = std::pair<CommandLineOption, CommandLineOptionType>;
};

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
