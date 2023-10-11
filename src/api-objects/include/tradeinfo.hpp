#pragma once

#include "cct_string.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "orderid.hpp"
#include "tradedamounts.hpp"
#include "tradeoptions.hpp"

namespace cct::api {

using UserRefInt = int32_t;

struct TradeContext {
  TradeContext(Market market, TradeSide s, UserRefInt userRef = 0) : mk(market), side(s), userRef(userRef) {}

  CurrencyCode fromCur() const { return side == TradeSide::kSell ? mk.base() : mk.quote(); }
  CurrencyCode toCur() const { return side == TradeSide::kBuy ? mk.base() : mk.quote(); }

  Market mk;
  TradeSide side;
  UserRefInt userRef;  // Used by Kraken for instance, used to group orders queries context
};

struct TradeInfo {
  TradeContext tradeContext;
  TradeOptions options;
};

struct OrderInfo {
  bool operator==(const OrderInfo &) const = default;

  TradedAmounts tradedAmounts;
  bool isClosed = false;
};

struct PlaceOrderInfo {
  bool isClosed() const { return orderInfo.isClosed; }
  void setClosed() { orderInfo.isClosed = true; }

  TradedAmounts &tradedAmounts() { return orderInfo.tradedAmounts; }
  const TradedAmounts &tradedAmounts() const { return orderInfo.tradedAmounts; }

  bool operator==(const PlaceOrderInfo &) const = default;

  using trivially_relocatable = is_trivially_relocatable<OrderId>::type;

  OrderInfo orderInfo;
  OrderId orderId;
};

}  // namespace cct::api