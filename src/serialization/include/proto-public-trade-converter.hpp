#pragma once

#include "market.hpp"
#include "public-trade.pb.h"
#include "publictrade.hpp"

namespace cct {

::proto::PublicTrade ConvertPublicTradeToProto(const PublicTrade &publicTrade);

class PublicTradeConverter {
 public:
  explicit PublicTradeConverter(Market market) : _market(market) {}

  PublicTrade operator()(const ::proto::PublicTrade &protoPublicTrade) const;

 private:
  Market _market;
};

}  // namespace cct