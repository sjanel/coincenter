#include "tradeoptions.hpp"

#include <stdexcept>

#include "cct_invalid_argument_exception.hpp"
#include "stringhelpers.hpp"
#include "unreachable.hpp"

namespace cct {

TradeOptions::TradeOptions(TradeTimeoutAction timeoutAction, TradeMode tradeMode, Duration dur,
                           Duration minTimeBetweenPriceUpdates, TradeType tradeType)
    : _maxTradeTime(dur),
      _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
      _timeoutAction(timeoutAction),
      _mode(tradeMode),
      _type(tradeType) {}

TradeOptions::TradeOptions(const PriceOptions &priceOptions, TradeTimeoutAction timeoutAction, TradeMode tradeMode,
                           Duration dur, Duration minTimeBetweenPriceUpdates, TradeType tradeType)
    : _maxTradeTime(dur),
      _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
      _priceOptions(priceOptions),
      _timeoutAction(timeoutAction),
      _mode(tradeMode),
      _type(tradeType) {}

std::string_view TradeOptions::timeoutActionStr() const {
  switch (_timeoutAction) {
    case TradeTimeoutAction::kCancel:
      return "cancel";
    case TradeTimeoutAction::kForceMatch:
      return "force-match";
    default:
      unreachable();
  }
}

string TradeOptions::str(bool placeRealOrderInSimulationMode) const {
  string ret;
  if (isSimulation()) {
    ret.append(placeRealOrderInSimulationMode ? "Real (unmatchable) " : "Simulated ");
  } else {
    ret.append("Real ");
  }
  ret.append(_priceOptions.str(placeRealOrderInSimulationMode));
  ret.append(", timeout of ");
  AppendString(ret, std::chrono::duration_cast<std::chrono::seconds>(_maxTradeTime).count());
  ret.append("s, ").append(timeoutActionStr()).append(" at timeout, min time between two price updates of ");
  AppendString(ret, std::chrono::duration_cast<std::chrono::seconds>(_minTimeBetweenPriceUpdates).count());
  ret.push_back('s');
  return ret;
}
}  // namespace cct