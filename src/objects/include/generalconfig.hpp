#pragma once

#include <string_view>

#include "apioutputtype.hpp"
#include "cct_json.hpp"
#include "logginginfo.hpp"
#include "timedef.hpp"

namespace cct {

class GeneralConfig {
 public:
  static constexpr std::string_view kFilename = "generalconfig.json";

  static json LoadFile(std::string_view dataDir);

  GeneralConfig() = default;

  GeneralConfig(LoggingInfo &&loggingInfo, Duration fiatConversionQueryRate, ApiOutputType apiOutputType);

  const LoggingInfo &loggingInfo() const { return _loggingInfo; }

  ApiOutputType apiOutputType() const { return _apiOutputType; }

  Duration fiatConversionQueryRate() const { return _fiatConversionQueryRate; }

 private:
  LoggingInfo _loggingInfo;
  Duration _fiatConversionQueryRate = std::chrono::hours(8);
  ApiOutputType _apiOutputType = ApiOutputType::kFormattedTable;
};

}  // namespace cct