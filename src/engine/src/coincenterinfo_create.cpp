#include "coincenterinfo_create.hpp"

#include <string_view>
#include <utility>

#include "apioutputtype.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "coincenteroptions.hpp"
#include "exchangesecretsinfo.hpp"
#include "file.hpp"
#include "general-config.hpp"
#include "loadconfiguration.hpp"
#include "logginginfo.hpp"
#include "monitoringinfo.hpp"
#include "runmodes.hpp"
#include "stringoptionparser.hpp"

namespace cct {

namespace {

schema::GeneralConfig LoadGeneralConfigAndOverrideOptionsFromCLI(const CoincenterCmdLineOptions &cmdLineOptions) {
  schema::GeneralConfig generalConfig = ReadGeneralConfig(cmdLineOptions.getDataDir());

  // Override general config options from CLI
  // Use at method to make sure to throw if key is not already present (it should)
  if (!cmdLineOptions.apiOutputType.empty()) {
    generalConfig.apiOutputType = ApiOutputTypeFromString(cmdLineOptions.apiOutputType);
  }
  if (!cmdLineOptions.logConsole.empty()) {
    generalConfig.log.consoleLevel = string(cmdLineOptions.logConsole);
  }
  if (!cmdLineOptions.logFile.empty()) {
    generalConfig.log.fileLevel = string(cmdLineOptions.logFile);
  }

  return generalConfig;
}

MonitoringInfo MonitoringInfo_Create(std::string_view programName, const CoincenterCmdLineOptions &cmdLineOptions) {
  return {cmdLineOptions.useMonitoring,      programName,
          cmdLineOptions.monitoringAddress,  cmdLineOptions.monitoringPort,
          cmdLineOptions.monitoringUsername, cmdLineOptions.monitoringPassword};
}

}  // namespace

CoincenterInfo CoincenterInfo_Create(std::string_view programName, const CoincenterCmdLineOptions &cmdLineOptions,
                                     settings::RunMode runMode) {
  const auto dataDir = cmdLineOptions.getDataDir();
  LoggingInfo loggingInfo(LoggingInfo::WithLoggersCreation::kNo, dataDir);

  schema::GeneralConfig generalConfig = LoadGeneralConfigAndOverrideOptionsFromCLI(cmdLineOptions);

  // Create LoggingInfo first as it is a RAII structure re-initializing spdlog loggers.
  // It will be held by GeneralConfig and then itself by CoincenterInfo though.
  loggingInfo = LoggingInfo(LoggingInfo::WithLoggersCreation::kYes, dataDir, generalConfig.log);

  const auto exchangeConfigFileType = runMode == settings::RunMode::kTestKeys
                                          ? LoadConfiguration::ExchangeConfigFileType::kTest
                                          : LoadConfiguration::ExchangeConfigFileType::kProd;

  const LoadConfiguration loadConfiguration(dataDir, exchangeConfigFileType);

  const File currencyAcronymsTranslatorFile(dataDir, File::Type::kStatic, "currencyacronymtranslator.json",
                                            File::IfError::kThrow);
  const File stableCoinsFile(dataDir, File::Type::kStatic, "stablecoins.json", File::IfError::kThrow);
  const File currencyPrefixesTranslatorFile(dataDir, File::Type::kStatic, "currency_prefix_translator.json",
                                            File::IfError::kThrow);

  return CoincenterInfo(runMode, loadConfiguration, std::move(generalConfig), std::move(loggingInfo),
                        MonitoringInfo_Create(programName, cmdLineOptions), currencyAcronymsTranslatorFile,
                        stableCoinsFile, currencyPrefixesTranslatorFile);
}

ExchangeSecretsInfo ExchangeSecretsInfo_Create(const CoincenterCmdLineOptions &cmdLineOptions) {
  if (cmdLineOptions.noSecrets) {
    StringOptionParser anyParser(*cmdLineOptions.noSecrets);
    return ExchangeSecretsInfo(anyParser.parseExchanges());
  }
  return {};
}

}  // namespace cct
