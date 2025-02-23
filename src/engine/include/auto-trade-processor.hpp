#pragma once

#include <array>
#include <compare>
#include <functional>
#include <unordered_map>

#include "auto-trade-config.hpp"
#include "cct_smallvector.hpp"
#include "cct_vector.hpp"
#include "exchange-names.hpp"
#include "exchange.hpp"
#include "market-trader-engine.hpp"
#include "market.hpp"
#include "timedef.hpp"

namespace cct {
class AutoTradeOptions;
class CoincenterInfo;

class AutoTradeProcessor {
 public:
  using MarketTraderEngines = std::array<std::unordered_map<Market, MarketTraderEngine>, kNbSupportedExchanges>;

  explicit AutoTradeProcessor(const AutoTradeOptions& autoTradeOptions);

  struct SelectedMarket {
    ExchangeNames privateExchangeNames;
    Market market;
  };

  using SelectedMarketVector = SmallVector<SelectedMarket, kTypicalNbPrivateAccounts>;

  SelectedMarketVector computeSelectedMarkets();

  MarketTraderEngines createMarketTraderEngines(const CoincenterInfo& coincenterInfo) const;

 private:
  struct MarketStatus {
    ExchangeNames privateExchangeNames;
    Market market;
    TimePoint lastQueryTime;
    const schema::AutoTradeMarketConfig* pMarketAutoTradeOptions{};
  };

  using MarketStatusVector = vector<MarketStatus>;

  struct ExchangeStatus {
    MarketStatusVector marketStatusVector;
    const schema::AutoTradeExchangeConfig* pPublicExchangeAutoTradeOptions{};
    ExchangeNameEnum exchangeNameEnum;
  };

  using ExchangeStatusVector = SmallVector<ExchangeStatus, kTypicalNbPrivateAccounts>;

  ExchangeStatusVector _exchangeStatusVector;
  TimePoint _startTs = Clock::now();
  TimePoint _ts{_startTs};
};
}  // namespace cct