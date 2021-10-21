#include "tradeoptions.hpp"

#include "cct_exception.hpp"

namespace cct {
namespace api {
namespace {
TradeStrategy StrategyFromStr(std::string_view strategyStr) {
  if (strategyStr == "maker") {
    return TradeStrategy::kMaker;
  }
  if (strategyStr == "adapt") {
    return TradeStrategy::kMakerThenTaker;
  }
  if (strategyStr == "taker") {
    return TradeStrategy::kTaker;
  }
  throw exception("Unrecognized trade strategy " + string(strategyStr));
}
}  // namespace

TradeOptions::TradeOptions(TradeStrategy tradeStrategy, TradeMode tradeMode, Clock::duration dur,
                           Clock::duration emergencyBufferTime, Clock::duration minTimeBetweenPriceUpdates)
    : _maxTradeTime(dur),
      _emergencyBufferTime(emergencyBufferTime),
      _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
      _strategy(tradeStrategy),
      _tradeMode(tradeMode) {}

TradeOptions::TradeOptions(std::string_view strategyStr, TradeMode tradeMode, Clock::duration dur,
                           Clock::duration emergencyBufferTime, Clock::duration minTimeBetweenPriceUpdates)
    : _maxTradeTime(dur),
      _emergencyBufferTime(emergencyBufferTime),
      _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
      _strategy(StrategyFromStr(strategyStr)),
      _tradeMode(tradeMode) {}

std::string_view TradeOptions::strategyStr() const {
  switch (_strategy) {
    case TradeStrategy::kMaker:
      return "maker";
    case TradeStrategy::kMakerThenTaker:
      return "adapt";
    case TradeStrategy::kTaker:
      return "taker";
    default:
      throw exception("Unexpected strategy value");
  }
}

string TradeOptions::str() const {
  string ret(isSimulation() ? "Simulated " : "Real ");
  ret.append(strategyStr());
  ret.append(" strategy, timeout of ");
  ret.append(std::to_string(std::chrono::duration_cast<std::chrono::seconds>(_maxTradeTime).count()));
  ret.append(" s, emergency time of ");
  ret.append(std::to_string(std::chrono::duration_cast<std::chrono::seconds>(_emergencyBufferTime).count()));
  ret.append(" s, min time between two limit price updates of ");
  ret.append(std::to_string(std::chrono::duration_cast<std::chrono::seconds>(_minTimeBetweenPriceUpdates).count()));
  ret.append(" s");
  return ret;
}
}  // namespace api
}  // namespace cct