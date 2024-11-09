#pragma once

#include "cct_type_traits.hpp"
#include "currencycodeset.hpp"

namespace cct::schema {

struct ExchangeAssetConfig {
  using trivially_relocatable = is_trivially_relocatable<CurrencyCodeSet>::type;

  void mergeWith(const ExchangeAssetConfig &other) {
    allExclude.insert(other.allExclude.begin(), other.allExclude.end());
    preferredPaymentCurrencies.insert(other.preferredPaymentCurrencies.begin(), other.preferredPaymentCurrencies.end());
    withdrawExclude.insert(other.withdrawExclude.begin(), other.withdrawExclude.end());
  }

  CurrencyCodeSet allExclude;
  CurrencyCodeSet preferredPaymentCurrencies;
  CurrencyCodeSet withdrawExclude;
};

}  // namespace cct::schema