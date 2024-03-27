#include "market-trader-factory.hpp"

#include <memory>
#include <string_view>

#include "cct_exception.hpp"
#include "dummy-market-trader.hpp"
#include "example-market-trader.hpp"

namespace cct {

class MarketTraderEngineState;

MarketTraderFactory::MarketTraderFactory(std::string_view algorithmName) : _algorithmName(algorithmName) {}

std::unique_ptr<AbstractMarketTrader> MarketTraderFactory::construct(
    const MarketTraderEngineState &marketTraderEngineState) const {
  if (_algorithmName == DummyMarketTrader::kName) {
    return std::make_unique<DummyMarketTrader>(marketTraderEngineState);
  }
  if (_algorithmName == ExampleMarketTrader::kName) {
    return std::make_unique<ExampleMarketTrader>(marketTraderEngineState);
  }
  throw exception("Unknown trader algorithm '{}' (among '{}')", _algorithmName, kAllAlgorithmsConcatenated);
}
}  // namespace cct