#include "processcommandsfromcli.hpp"

#include <string_view>

#include "cct_exception.hpp"
#include "coincenter-commands-processor.hpp"
#include "coincenter.hpp"
#include "coincenterinfo.hpp"
#include "coincenterinfo_create.hpp"
#include "coincenteroptions.hpp"
#include "curlhandle.hpp"
#include "runmodes.hpp"

namespace cct {

void ProcessCommandsFromCLI(std::string_view programName, const CoincenterCommands &coincenterCommands,
                            const CoincenterCmdLineOptions &generalOptions, settings::RunMode runMode) {
  // Should be outside the try / catch as it holds the RAII object managing the Logging (LoggingInfo)
  CoincenterInfo coincenterInfo = CoincenterInfo_Create(programName, generalOptions, runMode);

  CurlInitRAII curlInitRAII;  // Should be before any curl query

  try {
    Coincenter coincenter(coincenterInfo, ExchangeSecretsInfo_Create(generalOptions));
    CoincenterCommandsProcessor coincenterCommandsProcessor(coincenter);

    const auto nbCommandsProcessed = coincenterCommandsProcessor.process(coincenterCommands);

    if (nbCommandsProcessed != 0) {
      // Write potentially updated cache data on disk at end of program
      coincenter.updateFileCaches();
    }

    log::debug("normal termination after {} command(s) processed", nbCommandsProcessed);
  } catch (const exception &e) {
    // Log exception here as LoggingInfo is still configured at this point (will be destroyed immediately afterwards)
    log::critical("{}", e.what());
  }
}
}  // namespace cct