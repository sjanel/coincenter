#pragma once

#include <cstdint>
#include <type_traits>

#include "monetaryamount.hpp"
#include "optional-or-type.hpp"

namespace cct::schema {

namespace details {

template <bool Optional>
struct ExchangeTradeFeesConfig {
  template <class T, std::enable_if_t<std::is_same_v<T, ExchangeTradeFeesConfig<true>> && !Optional, bool> = true>
  void mergeWith(const T &other) {
    if (other.maker) {
      maker = *other.maker;
    }
    if (other.taker) {
      taker = *other.taker;
    }
  }

  enum class FeeType : int8_t { Maker, Taker };

  template <class MA, std::enable_if_t<std::is_same_v<MA, optional_or_t<MonetaryAmount, Optional>>, bool> = true>
  /// Apply the general maker fee defined for this exchange trade fees config on given MonetaryAmount.
  /// In other words, convert a gross amount into a net amount with maker fees
  MonetaryAmount applyFee(MA ma, FeeType feeType) const {
    return ma * (feeType == FeeType::Maker ? maker : taker);
  }

  optional_or_t<MonetaryAmount, Optional> maker;
  optional_or_t<MonetaryAmount, Optional> taker;
};

using ExchangeTradeFeesConfigOptional = ExchangeTradeFeesConfig<true>;

}  // namespace details

using ExchangeTradeFeesConfig = details::ExchangeTradeFeesConfig<false>;

}  // namespace cct::schema