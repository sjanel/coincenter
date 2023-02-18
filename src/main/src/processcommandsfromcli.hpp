#pragma once

#include <string_view>

#include "coincentercommands.hpp"
#include "coincenteroptions.hpp"

namespace cct {
void ProcessCommandsFromCLI(std::string_view programName, const CoincenterCommands &coincenterCommands,
                            const CoincenterCmdLineOptions &cmdLineOptions);
}