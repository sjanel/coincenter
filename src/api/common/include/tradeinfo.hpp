#pragma once

#include "cct_string.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "orderid.hpp"
#include "tradedamounts.hpp"
#include "tradeoptions.hpp"

namespace cct::api {

struct OrderRef {
  OrderRef(std::string_view idStr, int64_t nbSecondsSinceEpoch, Market market, TradeSide s)
      : id(idStr), userRef(nbSecondsSinceEpoch), m(market), side(s) {}

  CurrencyCode fromCur() const { return side == TradeSide::kSell ? m.base() : m.quote(); }
  CurrencyCode toCur() const { return side == TradeSide::kBuy ? m.base() : m.quote(); }

  string id;
  int64_t userRef;  // Used by Kraken for instance, used to group orders queries context
  Market m;
  TradeSide side;
};

struct TradeInfo {
#ifndef CCT_CTAD_SUPPORT
  TradeInfo(int64_t nbSecondsSinceEpoch, Market market, TradeSide s, const TradeOptions &opts)
      : userRef(nbSecondsSinceEpoch), m(market), side(s), options(opts) {}
#endif

  CurrencyCode fromCur() const { return side == TradeSide::kSell ? m.base() : m.quote(); }
  CurrencyCode toCur() const { return side == TradeSide::kBuy ? m.base() : m.quote(); }

  OrderRef createOrderRef(std::string_view id) const { return OrderRef(id, userRef, m, side); }

  int64_t userRef;  // Used by Kraken for instance, used to group orders queries context
  Market m;
  TradeSide side;
  TradeOptions options;
};

struct OrderInfo {
  explicit OrderInfo(TradedAmounts &&ta, bool closed = false) : tradedAmounts(std::move(ta)), isClosed(closed) {}

  void setClosed() { isClosed = true; }

  TradedAmounts tradedAmounts;
  bool isClosed;
};

struct PlaceOrderInfo {
  explicit PlaceOrderInfo(OrderInfo &&oInfo) : orderInfo(std::move(oInfo)) {}

  PlaceOrderInfo(OrderInfo &&oInfo, OrderId orderId) : orderInfo(std::move(oInfo)), orderId(std::move(orderId)) {}

  bool isClosed() const { return orderInfo.isClosed; }
  void setClosed() { orderInfo.setClosed(); }

  TradedAmounts &tradedAmounts() { return orderInfo.tradedAmounts; }
  const TradedAmounts &tradedAmounts() const { return orderInfo.tradedAmounts; }

  using trivially_relocatable = is_trivially_relocatable<OrderId>::type;

  OrderInfo orderInfo;
  OrderId orderId;
};

}  // namespace cct::api