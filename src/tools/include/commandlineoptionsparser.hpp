#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

#include "cct_flatset.hpp"
#include "cct_vector.hpp"

namespace cct {
using InvalidArgumentException = std::invalid_argument;

/// Simple Command line options parser.
/// Base taken from https://www.codeproject.com/Tips/5261900/Cplusplus-Lightweight-Parsing-Command-Line-Argumen
/// with some modifications. All credits to the original author.
/// License can be retrieved here: https://www.codeproject.com/info/cpol10.aspx
template <class Opts>
class CmdOpts : Opts {
 public:
  using MyProp = std::variant<std::string Opts::*, int Opts::*, double Opts::*, bool Opts::*>;
  using MyArg = std::pair<std::string, MyProp>;

  Opts parse(int argc, char* argv[]) { return parse(vector<std::string_view>(argv, argv + argc)); }

  Opts parse(const vector<std::string_view>& vargv) {
    int nbArgs = vargv.size();
    for (int idx = 0; idx < nbArgs; ++idx) {
      if (vargv[idx].starts_with('-') && !optionNames.contains(std::string(vargv[idx]))) {
        throw std::invalid_argument("unrecognized command-line option '" + std::string(vargv[idx]) + "'");
      }
      for (auto& cbk : callbacks) {
        cbk.second(idx, vargv);
      }
    }
    return static_cast<Opts>(*this);
  }

  static CmdOpts Create(std::initializer_list<MyArg> args) {
    CmdOpts cmdOpts;
    for (auto arg : args) {
      cmdOpts.register_callback(arg.first, arg.second);
      cmdOpts.optionNames.insert(arg.first);
    }
    return cmdOpts;
  }

 private:
  using callback_t = std::function<void(int, const vector<std::string_view>&)>;
  std::map<std::string, callback_t> callbacks;
  FlatSet<std::string> optionNames;

  CmdOpts() = default;

  CmdOpts(const CmdOpts&) = delete;
  CmdOpts(CmdOpts&&) = default;
  CmdOpts& operator=(const CmdOpts&) = delete;
  CmdOpts& operator=(CmdOpts&&) = default;

  void register_callback(std::string name, MyProp prop) {
    callbacks[name] = [this, name, prop](int idx, const vector<std::string_view>& argv) {
      if (argv[idx] == name) {
        visit(
            [this, idx, &argv, &name](auto&& arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, bool Opts::*>) {
                this->*arg = true;
              } else if constexpr (std::is_same_v<T, std::string Opts::*>) {
                if (idx + 1U < argv.size() && !argv[idx + 1].starts_with('-')) {
                  this->*arg = argv[idx + 1];
                } else {
                  throw std::invalid_argument("Expecting a value for option '" + name + "'");
                }
              } else if (idx + 1 < static_cast<int>(argv.size())) {
                std::stringstream value;
                value << argv[idx + 1];
                value >> this->*arg;
              } else {
                throw std::invalid_argument("Expecting a value for option '" + name + "'");
              }
            },
            prop);
      }
    };
  };
};
}  // namespace cct
