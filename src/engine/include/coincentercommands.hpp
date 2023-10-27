#pragma once

#include <span>
#include <string_view>

#include "cct_vector.hpp"
#include "coincentercommand.hpp"
#include "coincenteroptions.hpp"
#include "monitoringinfo.hpp"
#include "timedef.hpp"

namespace cct {

class CoincenterCommands {
 public:
  // Builds a CoincenterCommands without any commands.
  CoincenterCommands() noexcept = default;

  // Builds a CoincenterCommands and add commands from given command line options.
  explicit CoincenterCommands(const CoincenterCmdLineOptions &cmdLineOptions)
      : CoincenterCommands(std::span<const CoincenterCmdLineOptions>{&cmdLineOptions, 1U}) {}

  // Builds a CoincenterCommands and add commands from given command line options span.
  explicit CoincenterCommands(std::span<const CoincenterCmdLineOptions> cmdLineOptionsSpan);

  static vector<CoincenterCmdLineOptions> ParseOptions(int argc, const char *argv[]);

  /// @brief Set this CoincenterCommands from given command line options.
  /// @return false if only help or version is asked, true otherwise
  bool addOption(const CoincenterCmdLineOptions &cmdLineOptions);

  std::span<const CoincenterCommand> commands() const { return _commands; }

  Duration repeatTime() const { return _repeatTime; }

  int repeats() const { return _repeats; }

 private:
  using Commands = vector<CoincenterCommand>;

  Commands _commands;
  Duration _repeatTime{};
  int _repeats = 1;
};

}  // namespace cct
