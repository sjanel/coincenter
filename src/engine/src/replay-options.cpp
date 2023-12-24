#include "replay-options.hpp"

#include <string_view>

#include "dummy-market-trader.hpp"
#include "time-window.hpp"

namespace cct {

ReplayOptions::ReplayOptions(TimeWindow timeWindow, std::string_view algorithmNames, ReplayMode replayMode)
    : _timeWindow(timeWindow), _algorithmNames(algorithmNames), _replayMode(replayMode) {}

std::string_view ReplayOptions::algorithmNames() const {
  if (_replayMode == ReplayMode::kValidateOnly) {
    return DummyMarketTrader::kName;
  }
  return _algorithmNames;
}

}  // namespace cct