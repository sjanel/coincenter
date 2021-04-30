#pragma once

#include "monetaryamount.hpp"

namespace cct {
namespace api {
struct TradedOrdersInfo {
  TradedOrdersInfo(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode)
      : tradedFrom("0", fromCurrencyCode), tradedTo("0", toCurrencyCode) {}

  TradedOrdersInfo(MonetaryAmount fromAmount, MonetaryAmount toAmount) : tradedFrom(fromAmount), tradedTo(toAmount) {}

  TradedOrdersInfo operator+(const TradedOrdersInfo &o) const;
  TradedOrdersInfo &operator+=(const TradedOrdersInfo &o) {
    *this = *this + o;
    return *this;
  }

  bool isZero() const { return tradedFrom.isZero() && tradedTo.isZero(); }

  MonetaryAmount tradedFrom;  // In currency of 'from' amount
  MonetaryAmount tradedTo;    // In the opposite currency
};
}  // namespace api
}  // namespace cct