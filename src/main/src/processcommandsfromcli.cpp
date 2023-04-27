#include "processcommandsfromcli.hpp"

#include "coincenter.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"
#include "durationstring.hpp"
#include "file.hpp"
#include "generalconfig.hpp"
#include "loadconfiguration.hpp"
#include "logginginfo.hpp"
#include "stringoptionparser.hpp"

namespace cct {

namespace {
json LoadGeneralConfigAndOverrideOptionsFromCLI(const CoincenterCmdLineOptions &cmdLineOptions) {
  json generalConfigData = GeneralConfig::LoadFile(cmdLineOptions.dataDir);

  // Override general config options from CLI
  if (!cmdLineOptions.apiOutputType.empty()) {
    generalConfigData["apiOutputType"] = cmdLineOptions.apiOutputType;
  }
  if (!cmdLineOptions.logConsole.empty()) {
    generalConfigData["log"]["console"] = string(cmdLineOptions.logConsole);
  }
  if (!cmdLineOptions.logFile.empty()) {
    generalConfigData["log"]["file"] = string(cmdLineOptions.logFile);
  }

  return generalConfigData;
}
}  // namespace

void ProcessCommandsFromCLI(std::string_view programName, const CoincenterCommands &coincenterCommands,
                            const CoincenterCmdLineOptions &cmdLineOptions, settings::RunMode runMode) {
  json generalConfigData = LoadGeneralConfigAndOverrideOptionsFromCLI(cmdLineOptions);

  Duration fiatConversionQueryRate = ParseDuration(generalConfigData["fiatConversion"]["rate"].get<std::string_view>());
  ApiOutputType apiOutputType = ApiOutputTypeFromString(generalConfigData["apiOutputType"].get<std::string_view>());

  // Create LoggingInfo first as it is a RAII structure re-initializing spdlog loggers.
  // It will be held by GeneralConfig and then itself by CoincenterInfo though.
  LoggingInfo loggingInfo(static_cast<const json &>(generalConfigData["log"]));

  GeneralConfig generalConfig(std::move(loggingInfo), fiatConversionQueryRate, apiOutputType);

  LoadConfiguration loadConfiguration(cmdLineOptions.dataDir, LoadConfiguration::ExchangeConfigFileType::kProd);

  auto dataDir = loadConfiguration.dataDir();

  File currencyAcronymsTranslatorFile(dataDir, File::Type::kStatic, "currencyacronymtranslator.json",
                                      File::IfError::kThrow);
  File stableCoinsFile(dataDir, File::Type::kStatic, "stablecoins.json", File::IfError::kThrow);
  File currencyPrefixesTranslatorFile(dataDir, File::Type::kStatic, "currency_prefix_translator.json",
                                      File::IfError::kThrow);

  // Should be outside the try / catch as it holds the RAII object managing the Logging (LoggingInfo)
  CoincenterInfo coincenterInfo(runMode, loadConfiguration, std::move(generalConfig),
                                CoincenterCommands::CreateMonitoringInfo(programName, cmdLineOptions),
                                currencyAcronymsTranslatorFile, stableCoinsFile, currencyPrefixesTranslatorFile);

  ExchangeSecretsInfo exchangesSecretsInfo;
  if (cmdLineOptions.noSecrets) {
    StringOptionParser anyParser(*cmdLineOptions.noSecrets);

    exchangesSecretsInfo = ExchangeSecretsInfo(anyParser.getExchanges());
  }

  CurlInitRAII curlInitRAII;  // Should be before any curl query

  try {
    Coincenter coincenter(coincenterInfo, exchangesSecretsInfo);

    int nbCommandsProcessed = coincenter.process(coincenterCommands);

    if (nbCommandsProcessed > 0) {
      // Write potentially updated cache data on disk at end of program
      coincenter.updateFileCaches();
    }

    log::debug("normal termination after {} command(s) processed", nbCommandsProcessed);
  } catch (const exception &e) {
    // Log exception here as LoggingInfo is still configured at this point (will be destroyed immediately afterwards)
    log::critical("Exception: {}", e.what());
    throw e;
  }
}
}  // namespace cct