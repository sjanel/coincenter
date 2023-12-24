#pragma once

#include <utility>

#include "automation-config.hpp"

namespace cct {
class TradingConfig {
 public:
  TradingConfig() noexcept = default;

  TradingConfig(AutomationConfig automationConfig) : _automationConfig(std::move(automationConfig)) {}

  const AutomationConfig &automationConfig() const { return _automationConfig; }

 private:
  AutomationConfig _automationConfig;
};
}  // namespace cct