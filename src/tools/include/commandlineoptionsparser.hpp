#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

#include "cct_flatset.hpp"

namespace cct {
using InvalidArgumentException = std::invalid_argument;

/// Description of a command line option.
class CommandLineOption {
 public:
  using GroupNameAndPrio = std::pair<std::string_view, int>;

  CommandLineOption(GroupNameAndPrio optionGroupName, std::string_view fullName, char shortName,
                    std::string_view valueDescription, std::string_view description)
      : _optionGroupName(optionGroupName.first),
        _fullName(fullName),
        _valueDescription(valueDescription),
        _description(description),
        _prio(optionGroupName.second),
        _shortName(shortName) {}

  CommandLineOption(GroupNameAndPrio optionGroupName, std::string_view fullName, std::string_view valueDescription,
                    std::string_view description)
      : CommandLineOption(optionGroupName, fullName, '\0', valueDescription, description) {}

  bool matches(std::string_view optName) const {
    if (optName.size() == 2 && optName.front() == '-' && optName.back() == _shortName) {
      return true;
    }
    return optName == _fullName;
  }

  const std::string& optionGroupName() const { return _optionGroupName; }
  const std::string& fullName() const { return _fullName; }
  const std::string& description() const { return _description; }
  const std::string& valueDescription() const { return _valueDescription; }

  std::string shortName() const {
    std::string ret;
    if (_shortName != '\0') {
      ret.push_back('-');
      ret.push_back(_shortName);
    }
    return ret;
  }

  bool operator<(const CommandLineOption& o) const {
    if (_prio != o._prio) {
      return _prio < o._prio;
    }
    if (_optionGroupName != o._optionGroupName) {
      return _optionGroupName < o._optionGroupName;
    }
    return _fullName < o._fullName;
  }

 private:
  std::string _optionGroupName;
  std::string _fullName;
  std::string _valueDescription;
  std::string _description;
  int _prio;
  char _shortName;
};

class CommandLineOptions {
 private:
  using OptionsSet = FlatSet<CommandLineOption>;

 public:
  using const_iterator = OptionsSet::const_iterator;
  using value_type = CommandLineOption;

  CommandLineOptions() = default;

  CommandLineOptions(std::initializer_list<CommandLineOption> init) : _opts(init.begin(), init.end()) {}

  CommandLineOptions(std::span<const CommandLineOption> init) : _opts(init.begin(), init.end()) {}

  const_iterator begin() const { return _opts.begin(); }
  const_iterator end() const { return _opts.end(); }

  void insert(const CommandLineOption& o) { _opts.insert(o); }
  void insert(CommandLineOption&& o) { _opts.insert(std::move(o)); }

  const_iterator insert(const_iterator hint, const CommandLineOption& o) { return _opts.insert(hint, o); }
  const_iterator insert(const_iterator hint, CommandLineOption&& o) { return _opts.insert(hint, std::move(o)); }

  void merge(const CommandLineOptions& o) { _opts.insert(o.begin(), o.end()); }

  template <typename StreamType>
  void print(StreamType& stream) const {
    if (_opts.empty()) {
      return;
    }
    int lenFirstRows = 0;
    constexpr int kMaxCharLine = 140;
    for (const CommandLineOption& opt : _opts) {
      int lenRows = opt.fullName().size() + opt.valueDescription().size() + 1;
      int shortNameSize = opt.shortName().size();
      if (shortNameSize > 0) {
        lenRows += shortNameSize + 2;
      }
      lenFirstRows = std::max(lenFirstRows, lenRows);
    }
    std::string_view currentGroup, previousGroup;
    for (const CommandLineOption& opt : _opts) {
      currentGroup = opt.optionGroupName();
      if (currentGroup != previousGroup) {
        stream << std::endl << ' ' << currentGroup << std::endl;
      }
      std::string firstRowsStr = opt.fullName();
      std::string shortName = opt.shortName();
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
      std::string spaces(linePos, ' ');
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

        linePos += breakPos + 1;
        descr.remove_prefix(breakPos + 1);
      }

      previousGroup = currentGroup;
    }

    stream << std::endl;
  }

 private:
  OptionsSet _opts;
};

/// Simple Command line options parser.
/// Base taken from https://www.codeproject.com/Tips/5261900/Cplusplus-Lightweight-Parsing-Command-Line-Argumen
/// with enhancements.
/// Original code license can be retrieved here: https://www.codeproject.com/info/cpol10.aspx
template <class Opts>
class CommandLineOptionsParser : Opts {
 public:
  using OptionType = std::variant<std::string Opts::*, int Opts::*, bool Opts::*>;
  using CommandLineOptionWithValue = std::pair<CommandLineOption, OptionType>;

  CommandLineOptionsParser(std::initializer_list<CommandLineOptionWithValue> init)
      : _commandLineOptionsWithValues(init.begin(), init.end()) {}

  explicit CommandLineOptionsParser(std::span<const CommandLineOptionWithValue> options)
      : _commandLineOptionsWithValues(options.begin(), options.end()) {}

  CommandLineOptionsParser(const CommandLineOptionsParser&) = delete;
  CommandLineOptionsParser(CommandLineOptionsParser&&) = default;
  CommandLineOptionsParser& operator=(const CommandLineOptionsParser&) = delete;
  CommandLineOptionsParser& operator=(CommandLineOptionsParser&&) = default;

  void insert(const CommandLineOptionWithValue& commandLineOptionWithValue) {
    _commandLineOptionsWithValues.insert(commandLineOptionWithValue);
  }
  void insert(CommandLineOptionWithValue&& commandLineOptionWithValue) {
    _commandLineOptionsWithValues.insert(std::move(commandLineOptionWithValue));
  }

  void merge(const CommandLineOptionsParser& o) {
    _commandLineOptionsWithValues.insert(o._commandLineOptionsWithValues.begin(),
                                         o._commandLineOptionsWithValues.end());
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
          throw InvalidArgumentException("unrecognized command-line option '" + std::string(argStr) + "'");
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
  void displayHelp(const char* programName, StreamType& stream) const {
    stream << "usage: " << programName << " [options]" << std::endl;
    stream << "Options:" << std::endl;

    CommandLineOptions commandLineOptions;
    std::transform(_commandLineOptionsWithValues.begin(), _commandLineOptionsWithValues.end(),
                   std::inserter(commandLineOptions, commandLineOptions.end()),
                   [](const CommandLineOptionWithValue& arg) { return arg.first; });

    commandLineOptions.print(stream);
  }

 private:
  using callback_t = std::function<void(int, std::span<const char*>)>;

  struct CompareCommandLineOptionWithValue {
    bool operator()(const CommandLineOptionWithValue& lhs, const CommandLineOptionWithValue& rhs) const {
      return lhs.first < rhs.first;
    }
  };

  using CommandLineOptionsWithValuesSet = FlatSet<CommandLineOptionWithValue, CompareCommandLineOptionWithValue>;

  CommandLineOptionsWithValuesSet _commandLineOptionsWithValues;
  std::map<CommandLineOption, callback_t> _callbacks;

  void register_callback(const CommandLineOption& commandLineOption, OptionType prop) {
    _callbacks[commandLineOption] = [this, &commandLineOption, prop](int idx, std::span<const char*> argv) {
      if (commandLineOption.matches(argv[idx])) {
        visit(
            [this, idx, argv, &commandLineOption](auto&& arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, bool Opts::*>) {
                this->*arg = true;
              } else if constexpr (std::is_same_v<T, std::string Opts::*>) {
                if (idx + 1U < argv.size() && argv[idx + 1][0] != '-') {
                  this->*arg = argv[idx + 1];
                } else {
                  throw InvalidArgumentException("Expecting a value for option '" + commandLineOption.fullName() + "'");
                }
              } else {
                if (idx + 1 < static_cast<int>(argv.size())) {
                  std::stringstream value;
                  value << argv[idx + 1];
                  value >> this->*arg;
                } else {
                  throw InvalidArgumentException("Expecting a value for option '" + commandLineOption.fullName() + "'");
                }
              }
            },
            prop);
      }
    };
  };
};
}  // namespace cct
