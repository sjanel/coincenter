#include "proto-public-trade-converter.hpp"

#include "monetaryamount.hpp"
#include "public-trade.pb.h"
#include "publictrade.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"
#include "unreachable.hpp"

namespace cct {

namespace {
::proto::TradeSide ConvertTradeSide(TradeSide tradeSide) {
  switch (tradeSide) {
    case TradeSide::buy:
      return ::proto::TRADE_BUY;
    case TradeSide::sell:
      return ::proto::TRADE_SELL;
    default:
      unreachable();
  }
}

TradeSide ConvertTradeSide(::proto::TradeSide tradeSide) {
  switch (tradeSide) {
    case ::proto::TRADE_BUY:
      return TradeSide::buy;
    case ::proto::TRADE_SELL:
      return TradeSide::sell;
    default:
      unreachable();
  }
}

}  // namespace

::proto::PublicTrade ConvertPublicTradeToProto(const PublicTrade &publicTrade) {
  ::proto::PublicTrade protoObj;

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

PublicTrade PublicTradeConverter::operator()(const ::proto::PublicTrade &protoPublicTrade) const {
  const MonetaryAmount amount(protoPublicTrade.volumeamount(), _market.base(), protoPublicTrade.volumenbdecimals());
  const MonetaryAmount price(protoPublicTrade.priceamount(), _market.quote(), protoPublicTrade.pricenbdecimals());
  const TimePoint timeStamp(milliseconds(protoPublicTrade.unixtimestampinms()));

  return {ConvertTradeSide(protoPublicTrade.tradeside()), amount, price, timeStamp};
}

}  // namespace cct
