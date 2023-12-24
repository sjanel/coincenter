#include "market-trader-factory.hpp"

#include <memory>
#include <span>
#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "dummy-market-trader.hpp"
#include "example-market-trader.hpp"

namespace cct {

class MarketTraderEngineState;

std::span<const std::string_view> MarketTraderFactory::allSupportedAlgorithms() const {
  static constexpr std::string_view kAllAlgorithms[] = {DummyMarketTrader::kName, ExampleMarketTrader::kName};
  return kAllAlgorithms;
}

std::unique_ptr<AbstractMarketTrader> MarketTraderFactory::construct(
    std::string_view algorithmName, const MarketTraderEngineState &marketTraderEngineState) const {
  if (algorithmName == DummyMarketTrader::kName) {
    return std::make_unique<DummyMarketTrader>(marketTraderEngineState);
  }

  if (algorithmName == ExampleMarketTrader::kName) {
    return std::make_unique<ExampleMarketTrader>(marketTraderEngineState);
  }

  throw invalid_argument("Unknown trader algorithm '{}'", algorithmName);
}

}  // namespace cct