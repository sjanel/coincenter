#pragma once

#include <algorithm>
#include <bitset>
#include <cassert>
#include <climits>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "commandlineoption.hpp"
#include "mathhelpers.hpp"
#include "stringhelpers.hpp"

namespace cct {

inline void ThrowExpectingValueException(const CommandLineOption& commandLineOption) {
  static const string ex = "Expecting a value for option: " + string(commandLineOption.fullName());
  throw InvalidArgumentException(ex.c_str());
}

// helper type for the visitor #4
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

/// Basically an extension of the std::optional class with an additional state.
/// Indeed, we want to distinguish the presence of the option with its optional value.
class CommandLineOptionalInt {
 public:
  enum class State : int8_t { kValueIsSet, kOptionPresent, kOptionNotPresent };

  constexpr CommandLineOptionalInt() = default;

  constexpr CommandLineOptionalInt(State state) : _state(state) {}

  constexpr CommandLineOptionalInt(int value) : _value(value), _state(State::kValueIsSet) {}

  int& operator*() {
    assert(isSet());
    return _value;
  }
  int operator*() const {
    assert(isSet());
    return _value;
  }

  bool isPresent() const { return _state == State::kOptionPresent || _state == State::kValueIsSet; }
  bool isSet() const { return _state == State::kValueIsSet; }

 private:
  int _value = 0;
  State _state = State::kOptionNotPresent;
};

/// Simple Command line options parser.
/// Base taken from https://www.codeproject.com/Tips/5261900/Cplusplus-Lightweight-Parsing-Command-Line-Argumen
/// with enhancements.
/// Original code license can be retrieved here: https://www.codeproject.com/info/cpol10.aspx
template <class Opts>
class CommandLineOptionsParser : private Opts {
 public:
  using Duration = CommandLineOption::Duration;
  using OptionType = std::variant<string Opts::*, std::optional<string> Opts::*, std::string_view Opts::*,
                                  std::optional<std::string_view> Opts::*, int Opts::*, CommandLineOptionalInt Opts::*,
                                  bool Opts::*, Duration Opts::*>;
  using CommandLineOptionWithValue = std::pair<CommandLineOption, OptionType>;

  CommandLineOptionsParser(std::initializer_list<CommandLineOptionWithValue> init)
      : CommandLineOptionsParser(std::span<const CommandLineOptionWithValue>(init.begin(), init.end())) {}

  explicit CommandLineOptionsParser(std::span<const CommandLineOptionWithValue> options)
      : _commandLineOptionsWithValues(options.begin(), options.end()) {
    checkDuplicatesAndSort();
  }

  CommandLineOptionsParser(const CommandLineOptionsParser&) = delete;
  CommandLineOptionsParser(CommandLineOptionsParser&&) noexcept = default;
  CommandLineOptionsParser& operator=(const CommandLineOptionsParser&) = delete;
  CommandLineOptionsParser& operator=(CommandLineOptionsParser&&) noexcept = default;

  void insert(const CommandLineOptionWithValue& commandLineOptionWithValue) {
    _commandLineOptionsWithValues.push_back(commandLineOptionWithValue);
    checkDuplicatesAndSort();
  }
  void insert(CommandLineOptionWithValue&& commandLineOptionWithValue) {
    _commandLineOptionsWithValues.push_back(std::move(commandLineOptionWithValue));
    checkDuplicatesAndSort();
  }

  void merge(const CommandLineOptionsParser& o) {
    _commandLineOptionsWithValues.insert(o._commandLineOptionsWithValues.begin(),
                                         o._commandLineOptionsWithValues.end());
    checkDuplicatesAndSort();
  }

  Opts parse(std::span<const char*> vargv) {
    // First register the callbacks
    for (const CommandLineOptionWithValue& arg : _commandLineOptionsWithValues) {
      register_callback(arg.first, arg.second);
    }

    vargv = vargv.last(vargv.size() - 1U);  // skip first argument which is program name
    const int vargvSize = static_cast<int>(vargv.size());
    for (int idxOpt = 0; idxOpt < vargvSize; ++idxOpt) {
      const char* argStr = vargv[idxOpt];
      const bool knownOption = std::ranges::any_of(_commandLineOptionsWithValues,
                                                   [argStr](const auto& opt) { return opt.first.matches(argStr); });
      if (!knownOption) {
        static const string ex = "Unrecognized command-line option: " + string(argStr);
        throw InvalidArgumentException(ex.c_str());
      }

      for (auto& cbk : _callbacks) {
        cbk.second(idxOpt, vargv);
      }
    }

    return static_cast<Opts>(*this);
  }

  Opts parse(int argc, const char* argv[]) { return parse(std::span(argv, argc)); }

  template <typename StreamType>
  void displayHelp(std::string_view programName, StreamType& stream) const {
    stream << "usage: " << programName << " <options>" << std::endl;
    if (_commandLineOptionsWithValues.empty()) {
      return;
    }
    stream << "Options:" << std::endl;
    int lenFirstRows = 0;
    static constexpr int kMaxCharLine = 120;
    for (const auto& [opt, v] : _commandLineOptionsWithValues) {
      int lenRows = static_cast<int>(opt.fullName().size() + opt.valueDescription().size() + 1);
      int shortNameSize = static_cast<int>(opt.shortName().size());
      if (shortNameSize > 0) {
        lenRows += shortNameSize + 2;
      }
      lenFirstRows = std::max(lenFirstRows, lenRows);
    }
    std::string_view currentGroup, previousGroup;
    for (const auto& [opt, v] : _commandLineOptionsWithValues) {
      currentGroup = opt.optionGroupName();
      if (currentGroup != previousGroup) {
        stream << std::endl << ' ' << currentGroup << std::endl;
      }
      string firstRowsStr(opt.fullName());
      string shortName = opt.shortName();
      if (!shortName.empty()) {
        firstRowsStr.push_back(',');
        firstRowsStr.push_back(' ');
        firstRowsStr.append(shortName);
      }
      firstRowsStr.push_back(' ');
      firstRowsStr.append(opt.valueDescription());
      firstRowsStr.insert(firstRowsStr.end(), lenFirstRows - firstRowsStr.size(), ' ');
      stream << "  " << firstRowsStr << ' ';

      std::string_view descr = opt.description();
      int linePos = lenFirstRows + 3;
      string spaces(linePos, ' ');
      while (!descr.empty()) {
        if (linePos + descr.size() <= kMaxCharLine) {
          stream << descr << std::endl;
          break;
        }
        std::size_t breakPos = descr.find_first_of(" \n");
        if (breakPos == std::string_view::npos) {
          stream << std::endl << spaces << descr << std::endl;
          break;
        }
        if (linePos + breakPos > kMaxCharLine) {
          stream << std::endl << spaces;
          linePos = lenFirstRows + 3;
        }
        stream << std::string_view(descr.data(), breakPos + 1);
        if (descr[breakPos] == '\n') {
          stream << spaces;
          linePos = lenFirstRows + 3;
        }

        linePos += static_cast<int>(breakPos) + 1;
        descr.remove_prefix(breakPos + 1);
      }

      previousGroup = currentGroup;
    }

    stream << std::endl;
  }

 private:
  using CallbackType = std::function<void(int&, std::span<const char*>)>;

  vector<CommandLineOptionWithValue> _commandLineOptionsWithValues;
  std::map<CommandLineOption, CallbackType> _callbacks;

  /// TODO: make this check constexpr would be great (but it's actually not trivial at all)
  void checkDuplicatesAndSort() {
    checkShortNamesDuplicates();

    // Check long names equality by sorting
    const auto compareByLongName = [](const CommandLineOptionWithValue& lhs, const CommandLineOptionWithValue& rhs) {
      return lhs.first.fullName() < rhs.first.fullName();
    };
    std::ranges::sort(_commandLineOptionsWithValues, compareByLongName);
    const auto equiFunc = [&compareByLongName](const auto& lhs, const auto& rhs) {
      return !compareByLongName(lhs, rhs) && !compareByLongName(rhs, lhs);
    };
    auto foundIt = std::ranges::adjacent_find(_commandLineOptionsWithValues, equiFunc);
    if (foundIt != _commandLineOptionsWithValues.end()) {
      // Some duplicate has been found - either with same short name character, or same full name option
      ThrowDuplicatedOptionsException(foundIt->first.fullName(), std::next(foundIt)->first.fullName());
    }

    // Finally, sort the options by their natural ordering
    std::ranges::sort(_commandLineOptionsWithValues,
                      [](const CommandLineOptionWithValue& lhs, const CommandLineOptionWithValue& rhs) {
                        return lhs.first < rhs.first;
                      });
  }

  void checkShortNamesDuplicates() const {
    // Check short names equality with a hashmap of presence
    std::bitset<ipow(2, CHAR_BIT)> charPresenceBmp;
    for (const auto& [f, s] : _commandLineOptionsWithValues) {
      if (f.hasShortName()) {
        char c = f.shortNameChar();
        if (charPresenceBmp[c]) {
          ThrowDuplicatedOptionsException(c);
        }
        charPresenceBmp.set(c);
      }
    }
  }

  static void ThrowDuplicatedOptionsException(char shortName) {
    static string errMsg("Options with same short name '");
    errMsg.push_back(shortName);
    errMsg.append("' have been found");
    throw InvalidArgumentException(errMsg.c_str());
  }

  static void ThrowDuplicatedOptionsException(std::string_view lhsName, std::string_view rhsName) {
    static string errMsg("Duplicated options '");
    errMsg.append(lhsName).append("' and '").append(rhsName).append("' have been found");
    throw InvalidArgumentException(errMsg.c_str());
  }

  static bool IsOptionValue(const char* argv) {
    if (argv[0] != '-') {
      return true;
    }
    char secondChar = argv[1];
    return secondChar >= '0' && secondChar <= '9';
  }

  void register_callback(const CommandLineOption& commandLineOption, OptionType prop) {
    _callbacks[commandLineOption] = [this, &commandLineOption, prop](int& idx, std::span<const char*> argv) {
      if (commandLineOption.matches(argv[idx])) {
        std::visit(overloaded{
                       [this](bool Opts::*arg) { this->*arg = true; },
                       [this, &idx, argv, &commandLineOption](int Opts::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           const char* beg = argv[idx + 1];
                           const char* end = beg + strlen(beg);
                           this->*arg = FromString<int>(std::string_view(beg, end));
                           ++idx;
                         } else {
                           ThrowExpectingValueException(commandLineOption);
                         }
                       },
                       [this, &idx, argv](CommandLineOptionalInt Opts::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           const char* beg = argv[idx + 1];
                           const char* end = beg + strlen(beg);
                           this->*arg = FromString<int>(std::string_view(beg, end));
                           ++idx;
                         } else {
                           this->*arg = CommandLineOptionalInt(CommandLineOptionalInt::State::kOptionPresent);
                         }
                       },
                       [this, &idx, argv, &commandLineOption](string Opts::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           this->*arg = argv[idx + 1];
                           ++idx;
                         } else {
                           ThrowExpectingValueException(commandLineOption);
                         }
                       },
                       [this, &idx, argv](std::optional<string> Opts::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           this->*arg = argv[idx + 1];
                           ++idx;
                         } else {
                           this->*arg = string();
                         }
                       },
                       [this, &idx, argv, &commandLineOption](std::string_view Opts::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           this->*arg = argv[idx + 1];
                           ++idx;
                         } else {
                           ThrowExpectingValueException(commandLineOption);
                         }
                       },
                       [this, &idx, argv](std::optional<std::string_view> Opts::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           this->*arg = argv[idx + 1];
                           ++idx;
                         } else {
                           this->*arg = std::string_view();
                         }
                       },
                       [this, &idx, argv, &commandLineOption](Duration Opts::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           this->*arg = CommandLineOption::ParseDuration(argv[idx + 1]);
                           ++idx;
                         } else {
                           ThrowExpectingValueException(commandLineOption);
                         }
                       },
                   },
                   prop);
      }
    };
  };
};
}  // namespace cct
