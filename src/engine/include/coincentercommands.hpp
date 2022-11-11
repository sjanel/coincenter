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
  CoincenterCommands() noexcept(std::is_nothrow_default_constructible_v<Commands>) = default;

  static CoincenterCmdLineOptions ParseOptions(int argc, const char *argv[]);

  static MonitoringInfo CreateMonitoringInfo(std::string_view programName,
                                             const CoincenterCmdLineOptions &cmdLineOptions);

  bool setFromOptions(const CoincenterCmdLineOptions &cmdLineOptions);

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
