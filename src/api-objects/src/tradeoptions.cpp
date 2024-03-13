#include "tradeoptions.hpp"

#include <string_view>

#include "cct_string.hpp"
#include "durationstring.hpp"
#include "exchangeconfig.hpp"
#include "priceoptions.hpp"
#include "timedef.hpp"
#include "tradedefinitions.hpp"
#include "unreachable.hpp"

namespace cct {

TradeOptions::TradeOptions(const PriceOptions &priceOptions, TradeTimeoutAction timeoutAction, TradeMode tradeMode,
                           Duration maxTradeTime, Duration minTimeBetweenPriceUpdates, TradeTypePolicy tradeTypePolicy,
                           TradeSyncPolicy tradeSyncPolicy)
    : _maxTradeTime(maxTradeTime),
      _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
      _priceOptions(priceOptions),
      _timeoutAction(timeoutAction),
      _tradeMode(tradeMode),
      _tradeTypePolicy(tradeTypePolicy),
      _tradeSyncPolicy(tradeSyncPolicy) {}

TradeOptions::TradeOptions(const TradeOptions &rhs, const ExchangeConfig &exchangeConfig)
    : _maxTradeTime(rhs._maxTradeTime == kUndefinedDuration ? exchangeConfig.tradeConfig().timeout()
                                                            : rhs._maxTradeTime),
      _minTimeBetweenPriceUpdates(rhs._minTimeBetweenPriceUpdates == kUndefinedDuration
                                      ? exchangeConfig.tradeConfig().minPriceUpdateDuration()
                                      : rhs._minTimeBetweenPriceUpdates),
      _priceOptions(rhs._priceOptions.isDefault() ? PriceOptions(exchangeConfig.tradeConfig()) : rhs._priceOptions),
      _timeoutAction(rhs._timeoutAction == TradeTimeoutAction::kDefault
                         ? exchangeConfig.tradeConfig().tradeTimeoutAction()
                         : rhs._timeoutAction),
      _tradeMode(rhs._tradeMode),
      _tradeTypePolicy(rhs._tradeTypePolicy),
      _tradeSyncPolicy(rhs._tradeSyncPolicy) {}

bool TradeOptions::isMultiTradeAllowed(bool multiTradeAllowedByDefault) const {
  switch (_tradeTypePolicy) {
    case TradeTypePolicy::kDefault:
      return multiTradeAllowedByDefault;
    case TradeTypePolicy::kForceMultiTrade:
      return true;
    case TradeTypePolicy::kForceSingleTrade:
      return false;
    default:
      unreachable();
  }
}

std::string_view TradeOptions::timeoutActionStr() const {
  switch (_timeoutAction) {
    case TradeTimeoutAction::kDefault:
      // Default will behave the same as cancel - this field is not publicly exposed
      [[fallthrough]];
    case TradeTimeoutAction::kCancel:
      return "cancel";
    case TradeTimeoutAction::kMatch:
      return "match";
    default:
      unreachable();
  }
}

std::string_view TradeOptions::tradeSyncPolicyStr() const {
  switch (_tradeSyncPolicy) {
    case TradeSyncPolicy::kSynchronous:
      return "synchronous";
    case TradeSyncPolicy::kAsynchronous:
      return "asynchronous";
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
  ret.append(", ");
  ret.append(tradeSyncPolicyStr());
  ret.append(" mode, timeout of ");
  ret.append(DurationToString(_maxTradeTime));
  ret.append(", ").append(timeoutActionStr()).append(" at timeout, min time between two price updates of ");
  ret.append(DurationToString(_minTimeBetweenPriceUpdates));
  return ret;
}
}  // namespace cct