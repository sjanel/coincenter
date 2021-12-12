#pragma once

#include "cct_string.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "tradedamounts.hpp"
#include "tradeoptions.hpp"

namespace cct::api {

using OrderId = string;

struct TradeInfo {
  TradeInfo(CurrencyCode fromCur, CurrencyCode toCur, Market market, const TradeOptions &opts, int64_t uRef)
      : fromCurrencyCode(fromCur), toCurrencyCode(toCur), m(market), options(opts), userRef(uRef) {}

  CurrencyCode fromCurrencyCode;
  CurrencyCode toCurrencyCode;
  Market m;
  TradeOptions options;
  int64_t userRef;  // Used by Kraken for instance, used to group orders queries context
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

  OrderInfo orderInfo;
  OrderId orderId;
};

}  // namespace cct::api