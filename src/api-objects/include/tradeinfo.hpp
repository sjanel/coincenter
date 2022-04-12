#pragma once

#include "cct_string.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "orderid.hpp"
#include "tradedamounts.hpp"
#include "tradeoptions.hpp"

namespace cct::api {

struct OrderRef {
#ifndef CCT_AGGR_INIT_CXX20
  OrderRef(const OrderId &orderId, int64_t nbSecondsSinceEpoch, Market market, TradeSide s)
      : id(orderId), userRef(nbSecondsSinceEpoch), m(market), side(s) {}
#endif

  CurrencyCode fromCur() const { return side == TradeSide::kSell ? m.base() : m.quote(); }
  CurrencyCode toCur() const { return side == TradeSide::kBuy ? m.base() : m.quote(); }

  const OrderId &id;
  int64_t userRef;  // Used by Kraken for instance, used to group orders queries context
  Market m;
  TradeSide side;
};

struct TradeInfo {
#ifndef CCT_AGGR_INIT_CXX20
  TradeInfo(int64_t nbSecondsSinceEpoch, Market market, TradeSide s, const TradeOptions &opts)
      : userRef(nbSecondsSinceEpoch), m(market), side(s), options(opts) {}
#endif

  CurrencyCode fromCur() const { return side == TradeSide::kSell ? m.base() : m.quote(); }
  CurrencyCode toCur() const { return side == TradeSide::kBuy ? m.base() : m.quote(); }

  OrderRef createOrderRef(const OrderId &orderId) const { return OrderRef(orderId, userRef, m, side); }

  int64_t userRef;  // Used by Kraken for instance, used to group orders queries context
  Market m;
  TradeSide side;
  TradeOptions options;
};

struct OrderInfo {
#ifndef CCT_AGGR_INIT_CXX20
  explicit OrderInfo(const TradedAmounts &ta, bool closed = false) : tradedAmounts(ta), isClosed(closed) {}
  explicit OrderInfo(TradedAmounts &&ta, bool closed = false) : tradedAmounts(std::move(ta)), isClosed(closed) {}
#endif

  bool operator==(const OrderInfo &) const = default;

  TradedAmounts tradedAmounts;
  bool isClosed = false;
};

struct PlaceOrderInfo {
#ifndef CCT_AGGR_INIT_CXX20
  PlaceOrderInfo(const OrderInfo &oInfo, const OrderId &orderId) : orderInfo(oInfo), orderId(orderId) {}
  PlaceOrderInfo(OrderInfo &&oInfo, OrderId &&orderId) : orderInfo(std::move(oInfo)), orderId(std::move(orderId)) {}
#endif

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