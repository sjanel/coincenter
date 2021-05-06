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
  throw exception("Unrecognized trade strategy " + std::string(strategyStr));
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

std::string TradeOptions::strategyStr() const {
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

std::string TradeOptions::str() const {
  std::string ret(isSimulation() ? "Simulated " : "Real ");
  ret.append(strategyStr());
  ret.append(" strategy, timeout of ");
  ret.append(std::to_string(std::chrono::duration_cast<std::chrono::seconds>(_maxTradeTime).count()));
  ret.append("s, emergency time of ");
  ret.append(std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(_emergencyBufferTime).count()));
  ret.append("ms, min time between two limit price updates of ");
  ret.append(
      std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(_minTimeBetweenPriceUpdates).count()));
  ret.append("ms");
  return ret;
}
}  // namespace api
}  // namespace cct