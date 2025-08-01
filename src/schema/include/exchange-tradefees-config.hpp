#pragma once

#include <cstdint>
#include <type_traits>

#include "monetaryamount.hpp"
#include "optional-or-type.hpp"

namespace cct::schema {

namespace details {

template <bool Optional>
struct ExchangeTradeFeesConfig {
  template <class T>
  void mergeWith(const T &other)
    requires(std::is_same_v<T, ExchangeTradeFeesConfig<true>> && !Optional)
  {
    if (other.maker) {
      maker = *other.maker;
    }
    if (other.taker) {
      taker = *other.taker;
    }
  }

  enum class FeeType : int8_t { Maker, Taker };

  template <class MA>
  /// Apply the general maker fee defined for this exchange trade fees config on given MonetaryAmount.
  /// In other words, convert a gross amount into a net amount with maker fees
  MonetaryAmount applyFee(MA ma, FeeType feeType) const
    requires(std::is_same_v<MA, optional_or_t<MonetaryAmount, Optional>>)
  {
    return (ma * (MonetaryAmount(100) - fee(feeType))) / 100;
  }

  auto fee(FeeType feeType) const { return feeType == FeeType::Maker ? maker : taker; }

  optional_or_t<MonetaryAmount, Optional> maker;
  optional_or_t<MonetaryAmount, Optional> taker;
};

using ExchangeTradeFeesConfigOptional = ExchangeTradeFeesConfig<true>;

}  // namespace details

using ExchangeTradeFeesConfig = details::ExchangeTradeFeesConfig<false>;

}  // namespace cct::schema