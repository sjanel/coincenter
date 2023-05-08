#include "coincenterinfo_create.hpp"

#include "durationstring.hpp"
#include "file.hpp"
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

MonitoringInfo MonitoringInfo_Create(std::string_view programName, const CoincenterCmdLineOptions &cmdLineOptions) {
  return {cmdLineOptions.useMonitoring,      programName,
          cmdLineOptions.monitoringAddress,  cmdLineOptions.monitoringPort,
          cmdLineOptions.monitoringUsername, cmdLineOptions.monitoringPassword};
}

}  // namespace

CoincenterInfo CoincenterInfo_Create(std::string_view programName, const CoincenterCmdLineOptions &cmdLineOptions,
                                     settings::RunMode runMode) {
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

  return CoincenterInfo(runMode, loadConfiguration, std::move(generalConfig),
                        MonitoringInfo_Create(programName, cmdLineOptions), currencyAcronymsTranslatorFile,
                        stableCoinsFile, currencyPrefixesTranslatorFile);
}

ExchangeSecretsInfo ExchangeSecretsInfo_Create(const CoincenterCmdLineOptions &cmdLineOptions) {
  if (cmdLineOptions.noSecrets) {
    StringOptionParser anyParser(*cmdLineOptions.noSecrets);

    return ExchangeSecretsInfo(anyParser.getExchanges());
  }
  return {};
}

}  // namespace cct