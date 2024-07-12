#include "market-trader-factory.hpp"

#include <chrono>
#include <memory>
#include <span>
#include <string_view>

#include "abstract-market-trader.hpp"
#include "buy-and-hold-trader.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "dummy-market-trader.hpp"
#include "example-market-trader.hpp"
#include "grid-trader.hpp"
#include "ma-crossover-trader.hpp"
#include "market-maker-trader.hpp"
#include "mean-reversion-trader.hpp"
#include "rsi-trader.hpp"

namespace cct {

class MarketTraderEngineState;

namespace {
// Parameter-variant preset names for the parametrized algorithms.
constexpr std::string_view kMeanReversionConservative = "mean-reversion-conservative";
constexpr std::string_view kMeanReversionAggressive = "mean-reversion-aggressive";
constexpr std::string_view kGridTight = "grid-tight";
constexpr std::string_view kGridWide = "grid-wide";
constexpr std::string_view kGridAggressive = "grid-aggressive";
constexpr std::string_view kGridMaker = "grid-maker";
constexpr std::string_view kGridMakerWide = "grid-maker-wide";
constexpr std::string_view kTrendSlow = "trend-slow";
constexpr std::string_view kTrendMedium = "trend-medium";
constexpr std::string_view kRsiFast = "rsi-fast";
constexpr std::string_view kBollinger = "bollinger";
}  // namespace

std::span<const std::string_view> MarketTraderFactory::allSupportedAlgorithms() const {
  static constexpr std::string_view kAllAlgorithms[] = {
      DummyMarketTrader::kName,
      ExampleMarketTrader::kName,
      BuyAndHoldMarketTrader::kName,
      MeanReversionMarketTrader::kName,
      kMeanReversionConservative,
      kMeanReversionAggressive,
      MarketMakerMarketTrader::kName,
      MovingAverageCrossoverMarketTrader::kName,
      GridMarketTrader::kName,
      kGridTight,
      kGridWide,
      kGridAggressive,
      kGridMaker,
      kGridMakerWide,
      kTrendSlow,
      kTrendMedium,
      RsiMarketTrader::kName,
      kRsiFast,
      kBollinger,
  };
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

  if (algorithmName == BuyAndHoldMarketTrader::kName) {
    return std::make_unique<BuyAndHoldMarketTrader>(marketTraderEngineState);
  }

  if (algorithmName == MeanReversionMarketTrader::kName) {
    return std::make_unique<MeanReversionMarketTrader>(marketTraderEngineState);
  }

  if (algorithmName == kMeanReversionConservative) {
    MeanReversionMarketTrader::Parameters parameters;
    parameters.lookback = std::chrono::hours(8);
    parameters.entryZ = 3.0;
    parameters.intensityPercent = 60;
    return std::make_unique<MeanReversionMarketTrader>(kMeanReversionConservative, marketTraderEngineState, parameters);
  }

  if (algorithmName == kMeanReversionAggressive) {
    MeanReversionMarketTrader::Parameters parameters;
    parameters.lookback = std::chrono::hours(2);
    parameters.entryZ = 1.5;
    parameters.intensityPercent = 100;
    return std::make_unique<MeanReversionMarketTrader>(kMeanReversionAggressive, marketTraderEngineState, parameters);
  }

  if (algorithmName == MarketMakerMarketTrader::kName) {
    return std::make_unique<MarketMakerMarketTrader>(marketTraderEngineState);
  }

  if (algorithmName == MovingAverageCrossoverMarketTrader::kName) {
    return std::make_unique<MovingAverageCrossoverMarketTrader>(marketTraderEngineState);
  }

  if (algorithmName == GridMarketTrader::kName) {
    return std::make_unique<GridMarketTrader>(marketTraderEngineState);
  }

  if (algorithmName == kGridTight) {
    GridMarketTrader::Parameters parameters;
    parameters.stepRatio = 0.005;
    return std::make_unique<GridMarketTrader>(kGridTight, marketTraderEngineState, parameters);
  }

  if (algorithmName == kGridWide) {
    GridMarketTrader::Parameters parameters;
    parameters.stepRatio = 0.02;
    return std::make_unique<GridMarketTrader>(kGridWide, marketTraderEngineState, parameters);
  }

  if (algorithmName == kGridAggressive) {
    GridMarketTrader::Parameters parameters;
    parameters.stepRatio = 0.01;
    parameters.intensityPercent = 50;
    return std::make_unique<GridMarketTrader>(kGridAggressive, marketTraderEngineState, parameters);
  }

  if (algorithmName == kGridMaker) {
    GridMarketTrader::Parameters parameters;
    parameters.stepRatio = 0.02;
    parameters.priceStrategy = PriceStrategy::maker;
    return std::make_unique<GridMarketTrader>(kGridMaker, marketTraderEngineState, parameters);
  }

  if (algorithmName == kGridMakerWide) {
    GridMarketTrader::Parameters parameters;
    parameters.stepRatio = 0.03;
    parameters.intensityPercent = 25;
    parameters.priceStrategy = PriceStrategy::maker;
    return std::make_unique<GridMarketTrader>(kGridMakerWide, marketTraderEngineState, parameters);
  }

  if (algorithmName == kTrendSlow) {
    MovingAverageCrossoverMarketTrader::Parameters parameters;
    parameters.shortWindow = std::chrono::hours(6);
    parameters.longWindow = std::chrono::hours(48);
    parameters.decisionInterval = std::chrono::hours(2);
    parameters.minSeparationRatio = 0.005;
    parameters.intensityPercent = 100;
    return std::make_unique<MovingAverageCrossoverMarketTrader>(kTrendSlow, marketTraderEngineState, parameters);
  }

  if (algorithmName == kTrendMedium) {
    MovingAverageCrossoverMarketTrader::Parameters parameters;
    parameters.shortWindow = std::chrono::hours(3);
    parameters.longWindow = std::chrono::hours(24);
    parameters.decisionInterval = std::chrono::hours(1);
    parameters.minSeparationRatio = 0.005;
    parameters.intensityPercent = 100;
    return std::make_unique<MovingAverageCrossoverMarketTrader>(kTrendMedium, marketTraderEngineState, parameters);
  }

  if (algorithmName == RsiMarketTrader::kName) {
    return std::make_unique<RsiMarketTrader>(marketTraderEngineState);
  }

  if (algorithmName == kRsiFast) {
    RsiMarketTrader::Parameters parameters;
    parameters.samplingPeriod = std::chrono::minutes(5);
    parameters.decisionInterval = std::chrono::minutes(5);
    parameters.oversold = 25.0;
    parameters.overbought = 75.0;
    return std::make_unique<RsiMarketTrader>(kRsiFast, marketTraderEngineState, parameters);
  }

  if (algorithmName == kBollinger) {
    // Short-term Bollinger-band mean-reversion, in maker to keep fees low.
    MeanReversionMarketTrader::Parameters parameters;
    parameters.lookback = std::chrono::hours(1);
    parameters.decisionInterval = std::chrono::minutes(1);
    parameters.entryZ = 2.0;
    parameters.intensityPercent = 90;
    parameters.priceStrategy = PriceStrategy::maker;
    return std::make_unique<MeanReversionMarketTrader>(kBollinger, marketTraderEngineState, parameters);
  }

  throw invalid_argument("Unknown trader algorithm '{}'", algorithmName);
}

}  // namespace cct
