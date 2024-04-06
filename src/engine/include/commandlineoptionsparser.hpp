#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <optional>
#include <ostream>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "cct_cctype.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_vector.hpp"
#include "commandlineoption.hpp"
#include "durationstring.hpp"
#include "levenshteindistancecalculator.hpp"
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
class CommandLineOptionsParser {
 public:
  using CommandLineOptionType = AllowedCommandLineOptionsBase<OptValueType>::CommandLineOptionType;
  using CommandLineOptionWithValue = AllowedCommandLineOptionsBase<OptValueType>::CommandLineOptionWithValue;
  using value_type = OptValueType;

  template <unsigned N>
  explicit CommandLineOptionsParser(const CommandLineOptionWithValue (&init)[N]) {
    append(init);
  }

  CommandLineOptionsParser& append(std::ranges::input_range auto&& opts) {
    const auto insertedIt = _opts.insert(_opts.end(), std::ranges::begin(opts), std::ranges::end(opts));
    const auto sortByFirst = [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; };

    std::sort(insertedIt, _opts.end(), sortByFirst);
    std::inplace_merge(_opts.begin(), insertedIt, _opts.end(), sortByFirst);

    return *this;
  }

  auto parse(std::span<const char* const> groupedArguments) {
    OptValueType data;

    registerCallbacks(data);

    const int nbArgs = static_cast<int>(groupedArguments.size());
    for (int argPos = 0; argPos < nbArgs; ++argPos) {
      std::string_view argStr(groupedArguments[argPos]);

      if (std::ranges::none_of(_opts, [argStr](const auto& opt) { return opt.first.matches(argStr); })) {
        invalidArgument(argStr);
      }

      for (auto& [_, callback] : _callbacks) {
        callback(argPos, groupedArguments);
      }
    }

    return data;
  }

  void displayHelp(std::string_view programName, std::ostream& stream) const {
    stream << "usage: " << programName << " <general options> [command(s)]\n";
    if (_opts.empty()) {
      return;
    }
    stream << "Options:\n";

    const int lenTabRow = computeLenTabRow();
    std::string_view previousGroup;
    for (const auto& [opt, pm] : _opts) {
      std::string_view currentGroup = opt.commandHeader().groupName();
      if (currentGroup != previousGroup) {
        stream << '\n' << ' ' << currentGroup << '\n';
        previousGroup = currentGroup;
      }
      if (opt.fullName()[0] != '-') {
        stream << '\n';
      }

      RowPrefix(opt, lenTabRow, stream);

      std::string_view descr = opt.description();
      int linePos = lenTabRow;
      while (!descr.empty()) {
        static constexpr std::string_view kSpaceOrNewLine = " \n";
        auto breakPos = descr.find_first_of(kSpaceOrNewLine);
        if (breakPos == std::string_view::npos) {
          if (linePos + descr.size() > kMaxCharLine) {
            stream << '\n';
            Spaces(lenTabRow, stream);
          }
          stream << descr << '\n';
          break;
        }
        if (linePos + breakPos > kMaxCharLine) {
          stream << '\n';
          Spaces(lenTabRow, stream);
          linePos = lenTabRow;
        }
        stream << descr.substr(0, breakPos + 1);
        if (descr[breakPos] == '\n') {
          Spaces(lenTabRow, stream);
          linePos = lenTabRow;
        } else {
          linePos += static_cast<int>(breakPos) + 1;
        }

        descr.remove_prefix(breakPos + 1);
      }
    }
  }

 private:
  static constexpr std::string_view kEmptyLine =
      "                                                                                                                "
      "        ";
  static constexpr int kMaxCharLine = kEmptyLine.length();

  static_assert(kMaxCharLine >= 80);

  template <class>
  friend class CommandLineOptionsParserIterator;

  using CallbackType = std::function<void(int&, std::span<const char* const>)>;

  [[nodiscard]] bool isOptionValue(std::string_view opt) const {
    return std::ranges::none_of(_opts, [opt](const auto& cmdLineOpt) { return cmdLineOpt.first.matches(opt); });
  }

  static bool AreAllDigits(std::string_view opt) { return std::ranges::all_of(opt, isdigit); }

  static bool IsOptionInt(std::string_view opt) {
    return ((opt[0] == '-' || opt[0] == '+') && std::all_of(std::next(opt.begin()), opt.end(), isdigit)) ||
           AreAllDigits(opt);
  }

  CallbackType registerCallback(const CommandLineOption& commandLineOption, CommandLineOptionType prop,
                                OptValueType& data) {
    return [this, &commandLineOption, prop, &data](int& idx, std::span<const char* const> argv) {
      if (!commandLineOption.matches(argv[idx])) {
        return;
      }

      std::visit(overloaded{
                     // integral value matcher including bool
                     [&data, &idx, argv, &commandLineOption](std::integral auto OptValueType::*arg) {
                       using IntType = std::remove_reference_t<decltype(data.*arg)>;
                       if constexpr (std::is_same_v<IntType, bool>) {
                         data.*arg = true;
                       } else {
                         if (idx + 1U < argv.size()) {
                           std::string_view opt(argv[idx + 1]);
                           if (IsOptionInt(opt)) {
                             data.*arg = FromString<IntType>(opt);
                             ++idx;
                             return;
                           }
                         }
                         ThrowExpectingValueException(commandLineOption);
                       }
                     },

                     // CommandLineOptionalInt32 value matcher
                     [&data, &idx, argv](CommandLineOptionalInt32 OptValueType::*arg) {
                       data.*arg = CommandLineOptionalInt32(CommandLineOptionalInt32::State::kOptionPresent);
                       if (idx + 1U < argv.size()) {
                         std::string_view opt(argv[idx + 1]);
                         if (IsOptionInt(opt)) {
                           data.*arg = FromString<int32_t>(opt);
                           ++idx;
                         }
                       }
                     },

                     // std::string_view value matcher
                     [&data, &idx, argv, &commandLineOption](std::string_view OptValueType::*arg) {
                       if (idx + 1U < argv.size()) {
                         data.*arg = std::string_view(argv[++idx]);
                         return;
                       }
                       ThrowExpectingValueException(commandLineOption);
                     },

                     // optional std::string_view value matcher
                     [this, &data, &idx, argv](std::optional<std::string_view> OptValueType::*arg) {
                       if (idx + 1U < argv.size() && this->isOptionValue(argv[idx + 1])) {
                         data.*arg = std::string_view(argv[idx + 1]);
                         ++idx;
                         return;
                       }
                       data.*arg = std::string_view();
                     },

                     // duration value matcher
                     [&data, &idx, argv, &commandLineOption](Duration OptValueType::*arg) {
                       if (idx + 1U < argv.size()) {
                         data.*arg = ParseDuration(argv[++idx]);
                         return;
                       }
                       ThrowExpectingValueException(commandLineOption);
                     },
                 },
                 prop);
    };
  }

  static std::ostream& RowPrefix(const CommandLineOption& opt, int lenFirstRows, std::ostream& stream) {
    stream << "  ";
    auto nbPrintedChars = opt.fullName().size();
    stream << opt.fullName();
    if (opt.hasShortName()) {
      static constexpr std::string_view kShortNameSep = ", -";
      stream << kShortNameSep;
      stream << opt.shortNameChar();
      nbPrintedChars += kShortNameSep.size() + 1;
    }
    stream << ' ';
    stream << opt.valueDescription();
    nbPrintedChars += opt.valueDescription().size();
    return Spaces(lenFirstRows - static_cast<int>(nbPrintedChars) - 3, stream);
  }

  static std::ostream& Spaces(int nbSpaces, std::ostream& stream) {
    return stream << std::string_view(kEmptyLine.data(), nbSpaces);
  }

  int computeLenTabRow() const {
    int lenFirstRows = 0;
    for (const auto& [opt, _] : _opts) {
      int lenRows = static_cast<int>(opt.fullName().size() + opt.valueDescription().size() + 1);
      if (opt.hasShortName()) {
        lenRows += 4;
      }
      lenFirstRows = std::max(lenFirstRows, lenRows);
    }
    return lenFirstRows + 3;
  }

  void invalidArgument(std::string_view argStr) const {
    const auto [possibleOptionIdx, minDistance] = minLevenshteinDistanceOpt(argStr);
    auto existingOptionStr = _opts[possibleOptionIdx].first.fullName();

    if (minDistance <= 2 ||
        minDistance < static_cast<decltype(minDistance)>(std::min(argStr.size(), existingOptionStr.size()) / 2)) {
      throw invalid_argument("Unrecognized command-line option '{}' - did you mean '{}'?", argStr, existingOptionStr);
    }
    throw invalid_argument("Unrecognized command-line option '{}'", argStr);
  }

  std::pair<int, int> minLevenshteinDistanceOpt(std::string_view argStr) const {
    vector<int> minDistancesToFullNameOptions(_opts.size());
    LevenshteinDistanceCalculator calc;
    std::ranges::transform(_opts, minDistancesToFullNameOptions.begin(),
                           [argStr, &calc](const auto opt) { return calc(opt.first.fullName(), argStr); });
    const auto optIt = std::ranges::min_element(minDistancesToFullNameOptions);
    return {optIt - minDistancesToFullNameOptions.begin(), *optIt};
  }

  void registerCallbacks(OptValueType& data) {
    _callbacks.reserve(_opts.size());
    _callbacks.clear();
    for (const auto& [cmdLineOption, prop] : _opts) {
      _callbacks[cmdLineOption] = registerCallback(cmdLineOption, prop, data);
    }
  }

  vector<CommandLineOptionWithValue> _opts;
  std::unordered_map<CommandLineOption, CallbackType> _callbacks;
};

}  // namespace cct
