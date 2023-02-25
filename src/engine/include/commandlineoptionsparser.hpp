#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "commandlineoption.hpp"
#include "durationstring.hpp"
#include "mathhelpers.hpp"
#include "stringhelpers.hpp"

namespace cct {

inline void ThrowExpectingValueException(const CommandLineOption& commandLineOption) {
  throw invalid_argument("Expecting a value for option {}", commandLineOption.fullName());
}

// helper type for the visitor #4
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <class OptValueType>
class CommandLineOptionsParser : private OptValueType {
 public:
  // TODO: Once clang implements P0634R3, remove 'typename' here
  using CommandLineOptionType = typename AllowedCommandLineOptionsBase<OptValueType>::CommandLineOptionType;
  using CommandLineOptionWithValue = typename AllowedCommandLineOptionsBase<OptValueType>::CommandLineOptionWithValue;

  template <unsigned N>
  explicit CommandLineOptionsParser(const CommandLineOptionWithValue (&init)[N])
      : _opts(std::begin(init), std::end(init)) {}

  template <unsigned N>
  void append(const CommandLineOptionWithValue (&opts)[N]) {
    _opts.append(std::begin(opts), std::end(opts));
  }

  OptValueType parse(std::span<const char*> vargv) {
    // First register the callbacks
    for (const CommandLineOptionWithValue& arg : _opts) {
      registerCallback(arg.first, arg.second);
    }

    vargv = vargv.last(vargv.size() - 1U);  // skip first argument which is program name
    const int vargvSize = static_cast<int>(vargv.size());
    for (int idxOpt = 0; idxOpt < vargvSize; ++idxOpt) {
      const char* argStr = vargv[idxOpt];
      if (std::ranges::none_of(_opts, [argStr](const auto& opt) { return opt.first.matches(argStr); })) {
        throw invalid_argument("Unrecognized command-line option {}", argStr);
      }

      for (auto& cbk : _callbacks) {
        cbk.second(idxOpt, vargv);
      }
    }

    return static_cast<OptValueType>(*this);
  }

  OptValueType parse(int argc, const char* argv[]) { return parse(std::span(argv, argc)); }

  template <typename StreamType>
  void displayHelp(std::string_view programName, StreamType& stream) {
    stream << "usage: " << programName << " <options>" << std::endl;
    if (_opts.empty()) {
      return;
    }
    stream << "Options:" << std::endl;

    std::ranges::sort(_opts, [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

    int lenFirstRows = 0;
    static constexpr int kMaxCharLine = 120;
    for (const auto& [opt, pm] : _opts) {
      int lenRows = static_cast<int>(opt.fullName().size() + opt.valueDescription().size() + 1);
      if (opt.hasShortName()) {
        lenRows += 4;
      }
      lenFirstRows = std::max(lenFirstRows, lenRows);
    }
    std::string_view currentGroup, previousGroup;
    for (const auto& [opt, pm] : _opts) {
      currentGroup = opt.optionGroupName();
      if (currentGroup != previousGroup) {
        stream << std::endl << ' ' << currentGroup << std::endl;
      }
      string firstRowsStr(opt.fullName());
      if (opt.hasShortName()) {
        firstRowsStr.append(", -");
        firstRowsStr.push_back(opt.shortNameChar());
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

  vector<CommandLineOptionWithValue> _opts;
  std::map<CommandLineOption, CallbackType> _callbacks;

  static bool IsOptionValue(const char* argv) { return argv[0] != '-' || isdigit(argv[1]); }

  void registerCallback(const CommandLineOption& commandLineOption, CommandLineOptionType prop) {
    _callbacks[commandLineOption] = [this, &commandLineOption, prop](int& idx, std::span<const char*> argv) {
      if (commandLineOption.matches(argv[idx])) {
        std::visit(overloaded{
                       [this](bool OptValueType::*arg) { this->*arg = true; },
                       [this, &idx, argv, &commandLineOption](int OptValueType::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           const char* beg = argv[idx + 1];
                           const char* end = beg + strlen(beg);
                           this->*arg = FromString<int>(std::string_view(beg, end));
                           ++idx;
                         } else {
                           ThrowExpectingValueException(commandLineOption);
                         }
                       },
                       [this, &idx, argv](CommandLineOptionalInt OptValueType::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           const char* beg = argv[idx + 1];
                           const char* end = beg + strlen(beg);
                           this->*arg = FromString<int>(std::string_view(beg, end));
                           ++idx;
                         } else {
                           this->*arg = CommandLineOptionalInt(CommandLineOptionalInt::State::kOptionPresent);
                         }
                       },
                       [this, &idx, argv, &commandLineOption](std::string_view OptValueType::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           this->*arg = argv[idx + 1];
                           ++idx;
                         } else {
                           ThrowExpectingValueException(commandLineOption);
                         }
                       },
                       [this, &idx, argv](std::optional<std::string_view> OptValueType::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           this->*arg = argv[idx + 1];
                           ++idx;
                         } else {
                           this->*arg = std::string_view();
                         }
                       },
                       [this, &idx, argv, &commandLineOption](Duration OptValueType::*arg) {
                         if (idx + 1U < argv.size() && IsOptionValue(argv[idx + 1])) {
                           this->*arg = ParseDuration(argv[idx + 1]);
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
