#pragma once

#include <string>

#include "market.hpp"
#include "monetaryamount.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {

using OrderId = std::string;

struct TradeInfo {
  TradeInfo(CurrencyCode fromCur, CurrencyCode toCur, Market market, const TradeOptions &opts, std::string &&uRef)
      : fromCurrencyCode(fromCur), toCurrencyCode(toCur), m(market), options(opts), userRef(std::move(uRef)) {}

  CurrencyCode fromCurrencyCode;
  CurrencyCode toCurrencyCode;
  Market m;
  TradeOptions options;
  std::string userRef;  // Only used for Kraken, used to group orders queries context
};

struct TradedAmounts {
  TradedAmounts(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode)
      : tradedFrom("0", fromCurrencyCode), tradedTo("0", toCurrencyCode) {}

  TradedAmounts(MonetaryAmount fromAmount, MonetaryAmount toAmount) : tradedFrom(fromAmount), tradedTo(toAmount) {}

  TradedAmounts operator+(const TradedAmounts &o) const;
  TradedAmounts &operator+=(const TradedAmounts &o) {
    *this = *this + o;
    return *this;
  }

  bool isZero() const { return tradedFrom.isZero() && tradedTo.isZero(); }

  std::string str() const;

  MonetaryAmount tradedFrom;  // In currency of 'from' amount
  MonetaryAmount tradedTo;    // In the opposite currency
};

struct OrderInfo {
  explicit OrderInfo(TradedAmounts &&ta, bool closed = false) : tradedAmounts(std::move(ta)), isClosed(closed) {}

  void setClosed() { isClosed = true; }

  TradedAmounts tradedAmounts;
  bool isClosed;
};

struct PlaceOrderInfo {
  explicit PlaceOrderInfo(OrderInfo &&oInfo) : orderInfo(std::move(oInfo)) {}

  PlaceOrderInfo(OrderInfo &&oInfo, std::string orderId) : orderInfo(std::move(oInfo)), orderId(std::move(orderId)) {}

  bool isClosed() const { return orderInfo.isClosed; }
  void setClosed() { orderInfo.setClosed(); }

  const TradedAmounts &tradedAmounts() const { return orderInfo.tradedAmounts; }
  TradedAmounts &tradedAmounts() { return orderInfo.tradedAmounts; }

  OrderInfo orderInfo;
  OrderId orderId;
};

}  // namespace api
}  // namespace cct