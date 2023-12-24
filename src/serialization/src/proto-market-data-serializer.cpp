#include "proto-market-data-serializer.hpp"

#include <string_view>

#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "proto-constants.hpp"
#include "proto-market-order-book.hpp"
#include "proto-public-trade.hpp"
#include "publictrade.hpp"
#include "serialization-tools.hpp"

namespace cct {

namespace {
MonetaryAmount PriceMonetaryAmount(const ::objects::TradeData& obj) {
  return MonetaryAmount(obj.priceamount(), CurrencyCode{}, obj.pricenbdecimals());
}

MonetaryAmount VolumeMonetaryAmount(const ::objects::TradeData& obj) {
  return MonetaryAmount(obj.volumeamount(), CurrencyCode{}, obj.volumenbdecimals());
}
}  // namespace

bool ProtoMarketDataSerializer::TradeDataComp::operator()(const ::objects::TradeData& lhs,
                                                          const ::objects::TradeData& rhs) const {
  if (lhs.unixtimestampinms() != rhs.unixtimestampinms()) {
    return lhs.unixtimestampinms() < rhs.unixtimestampinms();
  }
  MonetaryAmount lhsAmount = VolumeMonetaryAmount(lhs);
  MonetaryAmount rhsAmount = VolumeMonetaryAmount(rhs);
  if (lhsAmount != rhsAmount) {
    return lhsAmount < rhsAmount;
  }
  MonetaryAmount lhsPrice = PriceMonetaryAmount(lhs);
  MonetaryAmount rhsPrice = PriceMonetaryAmount(rhs);
  if (lhsPrice != rhsPrice) {
    return lhsPrice < rhsPrice;
  }
  if (lhs.tradeside() != rhs.tradeside()) {
    return lhs.tradeside() < rhs.tradeside();
  }
  return false;
}

bool ProtoMarketDataSerializer::TradeDataEqual::operator()(const ::objects::TradeData& lhs,
                                                           const ::objects::TradeData& rhs) const {
  return lhs.unixtimestampinms() == rhs.unixtimestampinms() && VolumeMonetaryAmount(lhs) == VolumeMonetaryAmount(rhs) &&
         PriceMonetaryAmount(lhs) == PriceMonetaryAmount(rhs) && lhs.tradeside() == rhs.tradeside();
}

ProtoMarketDataSerializer::ProtoMarketDataSerializer(std::string_view dataDir, std::string_view exchangeName)
    : _marketOrderBookAccumulator(ComputeProtoSubPath(dataDir, exchangeName, kSubPathMarketOrderBook), 1000),
      _tradesAccumulator(ComputeProtoSubPath(dataDir, exchangeName, kSubPathTrades), 25000) {}

void ProtoMarketDataSerializer::push(const MarketOrderBook& marketOrderBook) {
  if (!marketOrderBook.isValid()) {
    log::error("Do not serialize invalid market order book");
    return;
  }
  _marketOrderBookAccumulator.push(marketOrderBook.market(), CreateMarketOrderBookTimedData(marketOrderBook));
}

void ProtoMarketDataSerializer::push(Market market, std::span<const PublicTrade> publicTrades) {
  for (const auto& publicTrade : publicTrades) {
    if (!publicTrade.isValid()) {
      log::error("Do not serialize invalid public trade");
      continue;
    }
    _tradesAccumulator.push(market, ConvertPublicTradeToTradeData(publicTrade));
  }
}

}  // namespace cct