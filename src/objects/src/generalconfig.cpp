#include "generalconfig.hpp"

#include <string_view>
#include <utility>

#include "apioutputtype.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "file.hpp"
#include "generalconfigdefault.hpp"
#include "logginginfo.hpp"
#include "requestsconfig.hpp"
#include "timedef.hpp"
#include "trading-config.hpp"

namespace cct {

GeneralConfig::GeneralConfig(LoggingInfo &&loggingInfo, RequestsConfig &&requestsConfig, TradingConfig &&tradingConfig,
                             Duration fiatConversionQueryRate, ApiOutputType apiOutputType)
    : _loggingInfo(std::move(loggingInfo)),
      _requestsConfig(std::move(requestsConfig)),
      _tradingConfig(std::move(tradingConfig)),
      _fiatConversionQueryRate(fiatConversionQueryRate),
      _apiOutputType(apiOutputType) {}

json GeneralConfig::LoadFile(std::string_view dataDir) {
  File generalConfigFile(dataDir, File::Type::kStatic, GeneralConfig::kFilename, File::IfError::kNoThrow);
  json jsonData = GeneralConfigDefault::Prod();
  json generalConfigJsonData = generalConfigFile.readAllJson();
  if (generalConfigJsonData.empty()) {
    // Create a file with default values. User can then update them as he wishes.
    log::warn("No {} file found. Creating a default one which can be updated freely at your convenience",
              GeneralConfig::kFilename);
    generalConfigFile.write(jsonData);
  } else {
    jsonData.update(generalConfigJsonData, true);
    if (jsonData != generalConfigJsonData) {
      log::warn("File {} updated with default values of all supported options", GeneralConfig::kFilename);
      generalConfigFile.write(jsonData);
    }
  }
  return jsonData;
}

}  // namespace cct