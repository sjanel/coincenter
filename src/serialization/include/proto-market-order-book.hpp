#pragma once

#include "market-order-book-timed-data.pb.h"
#include "marketorderbook.hpp"

namespace cct {

::objects::MarketOrderBookTimedData CreateMarketOrderBookTimedData(const MarketOrderBook &marketOrderBook);

class MarketOrderBookConverter {
 public:
  explicit MarketOrderBookConverter(Market market) : _market(market) {}

  MarketOrderBook operator()(const ::objects::MarketOrderBookTimedData &marketOrderBookTimedData);

 private:
  Market _market;
};

}  // namespace cct