#pragma once

#include "market.hpp"
#include "publictrade.hpp"
#include "trade-data.pb.h"

namespace cct {

::objects::TradeData ConvertPublicTradeToTradeData(const PublicTrade &publicTrade);

class TradeDataToPublicTradeConverter {
 public:
  explicit TradeDataToPublicTradeConverter(Market market) : _market(market) {}

  PublicTrade operator()(const ::objects::TradeData &tradeData) const;

 private:
  Market _market;
};

}  // namespace cct