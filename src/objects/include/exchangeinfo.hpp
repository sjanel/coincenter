#pragma once

#include <chrono>
#include <string_view>

#include "cct_flatset.hpp"
#include "cct_json.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {
class ExchangeInfo {
 public:
  using CurrencySet = FlatSet<CurrencyCode>;
  using Clock = std::chrono::high_resolution_clock;

  ExchangeInfo(std::string_view exchangeNameStr, const json &exchangeData);

  /// Get a reference to the list of statically excluded currency codes to consider for the exchange,
  /// In both trading and withdrawal.
  const CurrencySet &excludedCurrenciesAll() const { return _excludedCurrenciesAll; }

  /// Get a reference to the list of statically excluded currency codes to consider for withdrawals.
  const CurrencySet &excludedCurrenciesWithdrawal() const { return _excludedCurrenciesWithdrawal; }

  /// Apply the general maker fee defined for this exchange on given MonetaryAmount.
  MonetaryAmount applyMakerFee(MonetaryAmount m) const { return m * _generalMakerRatio; }

  /// Apply the general taker fee defined for this exchange on given MonetaryAmount.
  MonetaryAmount applyTakerFee(MonetaryAmount m) const { return m * _generalTakerRatio; }

  /// Get the minimum time between two public api queries
  Clock::duration minPublicQueryDelay() const { return _minPublicQueryDelay; }

  /// Get the minimum time between two public api queries
  Clock::duration minPrivateQueryDelay() const { return _minPrivateQueryDelay; }

 private:
  CurrencySet _excludedCurrenciesAll;         // Currencies will be completely ignored by the exchange
  CurrencySet _excludedCurrenciesWithdrawal;  // Currencies unavailable for withdrawals
  Clock::duration _minPublicQueryDelay, _minPrivateQueryDelay;
  MonetaryAmount _generalMakerRatio;
  MonetaryAmount _generalTakerRatio;
};
}  // namespace cct
