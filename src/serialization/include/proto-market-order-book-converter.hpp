#pragma once

#include "market-order-book.pb.h"
#include "market.hpp"
#include "marketorderbook.hpp"

namespace cct {

::proto::MarketOrderBook ConvertMarketOrderBookToProto(const MarketOrderBook &marketOrderBook);

class MarketOrderBookConverter {
 public:
  explicit MarketOrderBookConverter(Market market) : _market(market) {}

  MarketOrderBook operator()(const ::proto::MarketOrderBook &marketOrderBookTimedData);

 private:
  Market _market;
};

}  // namespace cct