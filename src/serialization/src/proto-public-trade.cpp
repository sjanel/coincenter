#include "proto-public-trade.hpp"

#include "monetaryamount.hpp"
#include "publictrade.hpp"
#include "timedef.hpp"
#include "trade-data.pb.h"
#include "tradeside.hpp"
#include "unreachable.hpp"

namespace cct {
namespace {
::objects::TradeSide ConvertTradeSide(TradeSide tradeSide) {
  switch (tradeSide) {
    case TradeSide::kBuy:
      return ::objects::TRADE_BUY;
    case TradeSide::kSell:
      return ::objects::TRADE_SELL;
    default:
      unreachable();
  }
}

TradeSide ConvertTradeSide(::objects::TradeSide tradeSide) {
  switch (tradeSide) {
    case ::objects::TRADE_BUY:
      return TradeSide::kBuy;
    case ::objects::TRADE_SELL:
      return TradeSide::kSell;
    default:
      unreachable();
  }
}

}  // namespace

::objects::TradeData ConvertPublicTradeToTradeData(const PublicTrade &publicTrade) {
  ::objects::TradeData protoObj;

  protoObj.set_unixtimestampinms(TimestampToMillisecondsSinceEpoch(publicTrade.time()));

  const auto price = publicTrade.price();
  protoObj.set_priceamount(price.amount());
  protoObj.set_pricenbdecimals(price.nbDecimals());

  const auto volume = publicTrade.amount();
  protoObj.set_volumeamount(volume.amount());
  protoObj.set_volumenbdecimals(volume.nbDecimals());

  protoObj.set_tradeside(ConvertTradeSide(publicTrade.side()));

  return protoObj;
}

PublicTrade TradeDataToPublicTradeConverter::operator()(const ::objects::TradeData &tradeData) const {
  const MonetaryAmount amount(tradeData.volumeamount(), _market.base(), tradeData.volumenbdecimals());
  const MonetaryAmount price(tradeData.priceamount(), _market.quote(), tradeData.pricenbdecimals());
  const TimePoint timeStamp(milliseconds(tradeData.unixtimestampinms()));

  return {ConvertTradeSide(tradeData.tradeside()), amount, price, timeStamp};
}

}  // namespace cct
