#pragma once

#include <algorithm>
#include <bitset>
#include <climits>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <variant>

#include "cct_mathhelpers.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "commandlineoption.hpp"

namespace cct {

inline void ThrowExpectingValueException(const CommandLineOption& commandLineOption) {
  throw InvalidArgumentException("Expecting a value for option: " + string(commandLineOption.fullName()));
}

#ifndef _WIN32
// helper type for the visitor #4
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
#endif

/// Simple Command line options parser.
/// Base taken from https://www.codeproject.com/Tips/5261900/Cplusplus-Lightweight-Parsing-Command-Line-Argumen
/// with enhancements.
/// Original code license can be retrieved here: https://www.codeproject.com/info/cpol10.aspx
template <class Opts>
class CommandLineOptionsParser : private Opts {
 public:
  using Duration = CommandLineOption::Duration;
  using OptionType =
      std::variant<string Opts::*, std::optional<string> Opts::*, int Opts::*, bool Opts::*, Duration Opts::*>;
  using CommandLineOptionWithValue = std::pair<CommandLineOption, OptionType>;

  CommandLineOptionsParser(std::initializer_list<CommandLineOptionWithValue> init)
      : CommandLineOptionsParser(std::span<const CommandLineOptionWithValue>(init.begin(), init.end())) {}

  explicit CommandLineOptionsParser(std::span<const CommandLineOptionWithValue> options)
      : _commandLineOptionsWithValues(options.begin(), options.end()) {
    checkDuplicatesAndSort();
  }

  CommandLineOptionsParser(const CommandLineOptionsParser&) = delete;
  CommandLineOptionsParser(CommandLineOptionsParser&&) = default;
  CommandLineOptionsParser& operator=(const CommandLineOptionsParser&) = delete;
  CommandLineOptionsParser& operator=(CommandLineOptionsParser&&) = default;

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

    int idxOpt = 0;
    for (const char* argStr : vargv) {
      if (argStr[0] == '-') {
        const bool knownOption = std::any_of(_commandLineOptionsWithValues.begin(), _commandLineOptionsWithValues.end(),
                                             [argStr](const auto& opt) { return opt.first.matches(argStr); });
        if (!knownOption) {
          throw InvalidArgumentException("Unrecognized command-line option: " + string(argStr));
        }
      }
      for (auto& cbk : _callbacks) {
        cbk.second(idxOpt, vargv);
      }
      ++idxOpt;
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
    constexpr int kMaxCharLine = 140;
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
        stream << std::string_view(descr.begin(), descr.begin() + breakPos + 1);
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
  using callback_t = std::function<void(int, std::span<const char*>)>;

  vector<CommandLineOptionWithValue> _commandLineOptionsWithValues;
  std::map<CommandLineOption, callback_t> _callbacks;

  /// TODO: make this check constexpr would be great (but it's actually not trivial at all)
  void checkDuplicatesAndSort() {
    checkShortNamesDuplicates();

    // Check long names equality by sorting
    const auto compareByLongName = [](const CommandLineOptionWithValue& lhs, const CommandLineOptionWithValue& rhs) {
      return lhs.first.fullName() < rhs.first.fullName();
    };
    std::sort(_commandLineOptionsWithValues.begin(), _commandLineOptionsWithValues.end(), compareByLongName);
    const auto equiFunc = [&compareByLongName](const auto& lhs, const auto& rhs) {
      return !compareByLongName(lhs, rhs) && !compareByLongName(rhs, lhs);
    };
    auto foundIt =
        std::adjacent_find(_commandLineOptionsWithValues.begin(), _commandLineOptionsWithValues.end(), equiFunc);
    if (foundIt != _commandLineOptionsWithValues.end()) {
      // Some duplicate has been found - either with same short name character, or same full name option
      ThrowDuplicatedOptionsException(foundIt->first.fullName(), std::next(foundIt)->first.fullName());
    }

    // Finally, sort the options by their natural ordering
    std::sort(_commandLineOptionsWithValues.begin(), _commandLineOptionsWithValues.end(),
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
    string errMsg("Options with same short name '");
    errMsg.push_back(shortName);
    errMsg.append("' have been found");
    throw InvalidArgumentException(std::move(errMsg));
  }

  static void ThrowDuplicatedOptionsException(std::string_view lhsName, std::string_view rhsName) {
    string errMsg("Duplicated options '");
    errMsg.append(lhsName);
    errMsg.append("' and '");
    errMsg.append(rhsName);
    errMsg.append("' have been found");
    throw InvalidArgumentException(std::move(errMsg));
  }

#ifdef _WIN32
  // MSVC compiler bug. Information here:
  // https://developercommunity.visualstudio.com/t/Cannot-compile-lambda-with-pointer-to-me/1416679
  struct VisitFunc {
    VisitFunc(Opts* o, int i, std::span<const char*> a, const CommandLineOption& c)
        : opts(o), idx(i), argv(a), commandLineOption(c) {}

    void operator()(bool Opts::*arg) const { opts->*arg = true; }

    void operator()(int Opts::*arg) const {
      if (idx + 1 < static_cast<int>(argv.size())) {
        std::stringstream value;
        value << argv[idx + 1];
        value >> opts->*arg;
      } else {
        ThrowExpectingValueException(commandLineOption);
      }
    }

    void operator()(string Opts::*arg) const {
      if (idx + 1U < argv.size() && argv[idx + 1][0] != '-') {
        opts->*arg = argv[idx + 1];
      } else {
        ThrowExpectingValueException(commandLineOption);
      }
    }

    void operator()(std::optional<string> Opts::*arg) const {
      if (idx + 1U < argv.size() && argv[idx + 1][0] != '-') {
        opts->*arg = argv[idx + 1];
      } else {
        opts->*arg = string();
      }
    }

    void operator()(Duration Opts::*arg) const {
      if (idx + 1U < argv.size() && argv[idx + 1][0] != '-') {
        opts->*arg = CommandLineOption::ParseDuration(argv[idx + 1]);
      } else {
        ThrowExpectingValueException(commandLineOption);
      }
    }

    Opts* opts;
    int idx;
    std::span<const char*> argv;
    const CommandLineOption& commandLineOption;
  };
#endif

  void register_callback(const CommandLineOption& commandLineOption, OptionType prop) {
    _callbacks[commandLineOption] = [this, &commandLineOption, prop](int idx, std::span<const char*> argv) {
      if (commandLineOption.matches(argv[idx])) {
#ifdef _WIN32
        std::visit(
            VisitFunc {
              this, idx, argv, commandLineOption
#else
        std::visit(overloaded{
                       [this](bool Opts::*arg) { this->*arg = true; },
                       [this, idx, argv, &commandLineOption](int Opts::*arg) {
                         if (idx + 1 < static_cast<int>(argv.size())) {
                           std::stringstream value;
                           value << argv[idx + 1];
                           value >> this->*arg;
                         } else {
                           ThrowExpectingValueException(commandLineOption);
                         }
                       },
                       [this, idx, argv, &commandLineOption](string Opts::*arg) {
                         if (idx + 1U < argv.size() && argv[idx + 1][0] != '-') {
                           this->*arg = argv[idx + 1];
                         } else {
                           ThrowExpectingValueException(commandLineOption);
                         }
                       },
                       [this, idx, argv](std::optional<string> Opts::*arg) {
                         if (idx + 1U < argv.size() && argv[idx + 1][0] != '-') {
                           this->*arg = argv[idx + 1];
                         } else {
                           this->*arg = string();
                         }
                       },
                       [this, idx, argv, &commandLineOption](Duration Opts::*arg) {
                         if (idx + 1U < argv.size() && argv[idx + 1][0] != '-') {
                           this->*arg = CommandLineOption::ParseDuration(argv[idx + 1]);
                         } else {
                           ThrowExpectingValueException(commandLineOption);
                         }
                       },
#endif
            },
            prop);
      }
    };
  };
};
}  // namespace cct
