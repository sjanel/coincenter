#pragma once

#include <string_view>

#include "apioutputtype.hpp"
#include "cct_json.hpp"
#include "logginginfo.hpp"
#include "requestsconfig.hpp"
#include "timedef.hpp"
#include "trading-config.hpp"

namespace cct {

class GeneralConfig {
 public:
  static constexpr std::string_view kFilename = "generalconfig.json";

  static json LoadFile(std::string_view dataDir);

  GeneralConfig() = default;

  GeneralConfig(LoggingInfo &&loggingInfo, RequestsConfig &&requestsConfig, TradingConfig &&tradingConfig,
                Duration fiatConversionQueryRate, ApiOutputType apiOutputType);

  const LoggingInfo &loggingInfo() const { return _loggingInfo; }

  const RequestsConfig &requestsConfig() const { return _requestsConfig; }

  const TradingConfig &tradingConfig() const { return _tradingConfig; }

  ApiOutputType apiOutputType() const { return _apiOutputType; }

  Duration fiatConversionQueryRate() const { return _fiatConversionQueryRate; }

 private:
  LoggingInfo _loggingInfo{LoggingInfo::WithLoggersCreation::kYes};
  RequestsConfig _requestsConfig;
  TradingConfig _tradingConfig;
  Duration _fiatConversionQueryRate = std::chrono::hours(8);
  ApiOutputType _apiOutputType = ApiOutputType::kFormattedTable;
};

}  // namespace cct