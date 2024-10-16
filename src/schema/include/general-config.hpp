#pragma once

#include <chrono>
#include <string_view>

#include "apioutputtype.hpp"
#include "duration-schema.hpp"
#include "log-config.hpp"
#include "requests-config.hpp"
#include "trading-config.hpp"

namespace cct {

namespace schema {

struct FiatConversionConfig {
  Duration rate{std::chrono::hours(8)};
};

struct GeneralConfig {
  ApiOutputType apiOutputType{ApiOutputType::table};
  FiatConversionConfig fiatConversion;
  LogConfig log;
  RequestsConfig requests;
  TradingConfig trading;
};

}  // namespace schema

schema::GeneralConfig ReadGeneralConfig(std::string_view dataDir);

}  // namespace cct