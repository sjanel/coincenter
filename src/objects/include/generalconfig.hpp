#pragma once

#include <string_view>

#include "cct_json.hpp"
#include "logginginfo.hpp"
#include "timedef.hpp"

namespace cct {

class GeneralConfig {
 public:
  static constexpr std::string_view kFilename = "generalconfig.json";

  static json LoadFile(std::string_view dataDir);

  GeneralConfig() = default;

  GeneralConfig(const LoggingInfo &loggingInfo, Duration fiatConversionQueryRate, bool printResults);

  GeneralConfig(LoggingInfo &&loggingInfo, Duration fiatConversionQueryRate, bool printResults);

  const LoggingInfo &loggingInfo() const { return _loggingInfo; }

  bool printResults() const { return _printResults; }

  Duration fiatConversionQueryRate() const { return _fiatConversionQueryRate; }

 private:
  LoggingInfo _loggingInfo;
  Duration _fiatConversionQueryRate = std::chrono::hours(8);
  bool _printResults = true;
};

}  // namespace cct