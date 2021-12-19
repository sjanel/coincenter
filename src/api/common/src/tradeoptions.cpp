#include "tradeoptions.hpp"

#include "cct_exception.hpp"
#include "stringhelpers.hpp"
#include "unreachable.hpp"

namespace cct {
namespace {

constexpr std::string_view kMakerStr = "maker";
constexpr std::string_view kNibbleStr = "nibble";
constexpr std::string_view kTakerStr = "taker";

constexpr TradePriceStrategy StrategyFromStr(std::string_view priceStrategyStr) {
  if (priceStrategyStr.empty() || priceStrategyStr == kMakerStr) {
    return TradePriceStrategy::kMaker;
  }
  if (priceStrategyStr == kNibbleStr) {
    return TradePriceStrategy::kNibble;
  }
  if (priceStrategyStr == kTakerStr) {
    return TradePriceStrategy::kTaker;
  }
  throw exception("Unrecognized trade strategy " + string(priceStrategyStr));
}
}  // namespace

TradeOptions::TradeOptions(std::string_view priceStrategyStr, TradeTimeoutAction timeoutAction, TradeMode tradeMode,
                           Clock::duration dur, Clock::duration minTimeBetweenPriceUpdates, TradeType tradeType)
    : _maxTradeTime(dur),
      _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
      _priceStrategy(StrategyFromStr(priceStrategyStr)),
      _timeoutAction(timeoutAction),
      _mode(tradeMode),
      _type(tradeType) {}

std::string_view TradeOptions::priceStrategyStr(bool placeRealOrderInSimulationMode) const {
  if (placeRealOrderInSimulationMode) {
    return kMakerStr;
  }
  switch (_priceStrategy) {
    case TradePriceStrategy::kMaker:
      return kMakerStr;
    case TradePriceStrategy::kNibble:
      return kNibbleStr;
    case TradePriceStrategy::kTaker:
      return kTakerStr;
    default:
      unreachable();
  }
}

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
  ret.append(priceStrategyStr(placeRealOrderInSimulationMode));
  ret.append(" strategy, timeout of ");
  AppendString(ret, std::chrono::duration_cast<std::chrono::seconds>(_maxTradeTime).count());
  ret.append("s, ").append(timeoutActionStr()).append(" at timeout, min time between two price updates of ");
  AppendString(ret, std::chrono::duration_cast<std::chrono::seconds>(_minTimeBetweenPriceUpdates).count());
  ret.push_back('s');
  return ret;
}
}  // namespace cct