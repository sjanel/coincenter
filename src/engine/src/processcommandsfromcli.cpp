#include "processcommandsfromcli.hpp"

#include "coincenter.hpp"
#include "coincenterinfo.hpp"
#include "durationstring.hpp"
#include "generalconfig.hpp"
#include "loadconfiguration.hpp"
#include "logginginfo.hpp"
#include "stringoptionparser.hpp"

namespace cct {

namespace {
json LoadGeneralConfigAndOverrideOptionsFromCLI(const CoincenterCmdLineOptions &cmdLineOptions) {
  json generalConfigData = GeneralConfig::LoadFile(cmdLineOptions.dataDir);

  // Override general config options from CLI
  if (cmdLineOptions.printResults) {
    generalConfigData["printResults"] = true;
  } else if (cmdLineOptions.noPrintResults) {
    generalConfigData["printResults"] = false;
  }
  if (!cmdLineOptions.logLevel.empty()) {
    generalConfigData["log"]["level"] = string(cmdLineOptions.logLevel);
  }
  if (cmdLineOptions.logConsole) {
    generalConfigData["log"]["file"] = false;
  }
  if (cmdLineOptions.logFile) {
    generalConfigData["log"]["file"] = true;
  }

  return generalConfigData;
}
}  // namespace

void ProcessCommandsFromCLI(std::string_view programName, const CoincenterCommands &coincenterCommands,
                            const CoincenterCmdLineOptions &cmdLineOptions) {
  json generalConfigData = LoadGeneralConfigAndOverrideOptionsFromCLI(cmdLineOptions);

  Duration fiatConversionQueryRate = ParseDuration(generalConfigData["fiatConversion"]["rate"].get<std::string_view>());

  GeneralConfig generalConfig(LoggingInfo(static_cast<const json &>(generalConfigData["log"])), fiatConversionQueryRate,
                              generalConfigData["printResults"].get<bool>());

  LoadConfiguration loadConfiguration(cmdLineOptions.dataDir, LoadConfiguration::ExchangeConfigFileType::kProd);

  CoincenterInfo coincenterInfo(settings::RunMode::kProd, loadConfiguration, std::move(generalConfig),
                                coincenterCommands.createMonitoringInfo(programName, cmdLineOptions));

  ExchangeSecretsInfo exchangesSecretsInfo;
  if (cmdLineOptions.nosecrets) {
    StringOptionParser anyParser(*cmdLineOptions.nosecrets);

    exchangesSecretsInfo = ExchangeSecretsInfo(anyParser.getExchanges());
  }

  Coincenter coincenter(coincenterInfo, exchangesSecretsInfo);

  coincenter.process(coincenterCommands);

  coincenter.updateFileCaches();  // Write potentially updated cache data on disk at end of program
}
}  // namespace cct