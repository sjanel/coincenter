#pragma once

#include <cstdint>

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "coincentercommandtype.hpp"
#include "size-bytes-schema.hpp"

namespace cct {
namespace schema {

struct ActivityTrackingConfig {
  SmallVector<CoincenterCommandType, 8U> commandTypes{CoincenterCommandType::Trade, CoincenterCommandType::Buy,
                                                      CoincenterCommandType::Sell, CoincenterCommandType::Withdraw,
                                                      CoincenterCommandType::DustSweeper};
  string dateFileNameFormat{"%Y-%m"};
  bool withSimulatedCommands{false};
};

struct LogConfig {
  ActivityTrackingConfig activityTracking;
  string consoleLevel{"info"};
  string fileLevel{"debug"};
  SizeBytes maxFileSize{5 * 1024 * 1024};  // 5Mi
  int32_t maxNbFiles{20};
};
}  // namespace schema
}  // namespace cct