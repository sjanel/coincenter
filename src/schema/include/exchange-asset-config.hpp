#pragma once

#include "cct_type_traits.hpp"
#include "currencycodeset.hpp"
#include "currencycodevector.hpp"

namespace cct::schema {

struct ExchangeAssetConfig {
  using trivially_relocatable = is_trivially_relocatable<CurrencyCodeSet>::type;

  void mergeWith(const ExchangeAssetConfig &other) {
    allExclude.insert(other.allExclude.begin(), other.allExclude.end());
    preferredPaymentCurrencies.insert(preferredPaymentCurrencies.begin(), other.preferredPaymentCurrencies.begin(),
                                      other.preferredPaymentCurrencies.end());
    withdrawExclude.insert(other.withdrawExclude.begin(), other.withdrawExclude.end());
    preferredChains.insert(preferredChains.begin(), other.preferredChains.begin(), other.preferredChains.end());
  }

  CurrencyCodeSet allExclude;
  CurrencyCodeVector preferredPaymentCurrencies;
  CurrencyCodeSet withdrawExclude;
  // when there are several chains available for a currency, pick the first that matches this list
  // Set this to ensure same chains are used between exchanges.
  CurrencyCodeVector preferredChains;
};

}  // namespace cct::schema