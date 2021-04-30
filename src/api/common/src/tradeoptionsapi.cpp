#include "tradeoptionsapi.hpp"

#include "cct_exception.hpp"

namespace cct {
namespace api {
namespace {
TradeOptions::Strategy StrategyFromStr(std::string_view strategyStr) {
  if (strategyStr == "maker") {
    return TradeOptions::Strategy::kMaker;
  }
  if (strategyStr == "adapt") {
    return TradeOptions::Strategy::kMakerThenTaker;
  }
  if (strategyStr == "taker") {
    return TradeOptions::Strategy::kTaker;
  }
  throw exception("Unrecognized trade strategy " + std::string(strategyStr));
}
}  // namespace

TradeOptions::TradeOptions(Strategy strategy, Mode mode, Clock::duration dur, Clock::duration emergencyBufferTime,
                           Clock::duration minTimeBetweenPriceUpdates)
    : _maxTradeTime(dur),
      _emergencyBufferTime(emergencyBufferTime),
      _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
      _strategy(strategy),
      _simulationMode(mode == Mode::kSimulation) {}

TradeOptions::TradeOptions(std::string_view strategyStr, Mode mode, Clock::duration dur,
                           Clock::duration emergencyBufferTime, Clock::duration minTimeBetweenPriceUpdates)
    : _maxTradeTime(dur),
      _emergencyBufferTime(emergencyBufferTime),
      _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
      _strategy(StrategyFromStr(strategyStr)),
      _simulationMode(mode == Mode::kSimulation) {}

std::string TradeOptions::strategyStr() const {
  switch (_strategy) {
    case Strategy::kMaker:
      return "maker";
    case Strategy::kMakerThenTaker:
      return "adapt";
    case Strategy::kTaker:
      return "taker";
    default:
      throw exception("Unexpected strategy value");
  }
}

std::string TradeOptions::str() const {
  std::string ret(_simulationMode ? "Simulated " : "Real ");
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