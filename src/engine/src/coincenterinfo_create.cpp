#include "coincenterinfo_create.hpp"

#include <string_view>
#include <utility>

#include "apioutputtype.hpp"
#include "automation-config.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "coincenteroptions.hpp"
#include "durationstring.hpp"
#include "exchangesecretsinfo.hpp"
#include "file.hpp"
#include "generalconfig.hpp"
#include "loadconfiguration.hpp"
#include "logginginfo.hpp"
#include "monetaryamount.hpp"
#include "monitoringinfo.hpp"
#include "requestsconfig.hpp"
#include "runmodes.hpp"
#include "stringoptionparser.hpp"
#include "timedef.hpp"
#include "trading-config.hpp"

namespace cct {

namespace {

json LoadGeneralConfigAndOverrideOptionsFromCLI(const CoincenterCmdLineOptions &cmdLineOptions) {
  json generalConfigData = GeneralConfig::LoadFile(cmdLineOptions.getDataDir());

  // Override general config options from CLI
  // Use at method to make sure to throw if key is not already present (it should)
  if (!cmdLineOptions.apiOutputType.empty()) {
    generalConfigData.at("apiOutputType") = cmdLineOptions.apiOutputType;
  }
  if (!cmdLineOptions.logConsole.empty()) {
    generalConfigData.at("log").at(LoggingInfo::kJsonFieldConsoleLevelName) = string(cmdLineOptions.logConsole);
  }
  if (!cmdLineOptions.logFile.empty()) {
    generalConfigData.at("log").at(LoggingInfo::kJsonFieldFileLevelName) = string(cmdLineOptions.logFile);
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
  const auto dataDir = cmdLineOptions.getDataDir();
  LoggingInfo loggingInfo(LoggingInfo::WithLoggersCreation::kNo, dataDir);

  const json generalConfigData = LoadGeneralConfigAndOverrideOptionsFromCLI(cmdLineOptions);

  const Duration fiatConversionQueryRate =
      ParseDuration(generalConfigData.at("fiatConversion").at("rate").get<std::string_view>());
  const ApiOutputType apiOutputType =
      ApiOutputTypeFromString(generalConfigData.at("apiOutputType").get<std::string_view>());

  // Create LoggingInfo first as it is a RAII structure re-initializing spdlog loggers.
  // It will be held by GeneralConfig and then itself by CoincenterInfo though.
  const auto &logConfigJsonPart = static_cast<const json &>(generalConfigData.at("log"));
  loggingInfo = LoggingInfo(LoggingInfo::WithLoggersCreation::kYes, dataDir, logConfigJsonPart);

  RequestsConfig requestsConfig(
      generalConfigData.at("requests").at("concurrency").at("nbMaxParallelRequests").get<int>());

  const auto &automationJsonPart = generalConfigData.at("trading").at("automation");
  const auto &deserializationJsonPart = automationJsonPart.at("deserialization");
  const auto &startingContextJsonPart = automationJsonPart.at("startingContext");

  Duration loadChunkDuration = ParseDuration(deserializationJsonPart.at("loadChunkDuration").get<std::string_view>());
  MonetaryAmount startBaseAmountEquivalent{
      startingContextJsonPart.at("startBaseAmountEquivalent").get<std::string_view>()};
  MonetaryAmount startQuoteAmountEquivalent{
      startingContextJsonPart.at("startQuoteAmountEquivalent").get<std::string_view>()};

  AutomationConfig automationConfig(loadChunkDuration, startBaseAmountEquivalent, startQuoteAmountEquivalent);
  TradingConfig tradingConfig(std::move(automationConfig));

  GeneralConfig generalConfig(std::move(loggingInfo), std::move(requestsConfig), std::move(tradingConfig),
                              fiatConversionQueryRate, apiOutputType);

  const LoadConfiguration loadConfiguration(dataDir, LoadConfiguration::ExchangeConfigFileType::kProd);

  const File currencyAcronymsTranslatorFile(dataDir, File::Type::kStatic, "currencyacronymtranslator.json",
                                            File::IfError::kThrow);
  const File stableCoinsFile(dataDir, File::Type::kStatic, "stablecoins.json", File::IfError::kThrow);
  const File currencyPrefixesTranslatorFile(dataDir, File::Type::kStatic, "currency_prefix_translator.json",
                                            File::IfError::kThrow);

  return CoincenterInfo(runMode, loadConfiguration, std::move(generalConfig),
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
