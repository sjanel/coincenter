#pragma once

#include <chrono>
#include <span>
#include <string_view>

#include "cct_flatset.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {
class ExchangeInfo {
 public:
  using CurrencySet = FlatSet<CurrencyCode>;
  using Clock = std::chrono::high_resolution_clock;

  enum struct FeeType { kMaker, kTaker };

  ExchangeInfo(std::string_view exchangeNameStr, std::string_view makerStr, std::string_view takerStr,
               std::span<const CurrencyCode> excludedAllCurrencies,
               std::span<const CurrencyCode> excludedCurrenciesWithdraw, int minPublicQueryDelayMs,
               int minPrivateQueryDelayMs, bool validateDepositAddressesInFile);

  /// Get a reference to the list of statically excluded currency codes to consider for the exchange,
  /// In both trading and withdrawal.
  const CurrencySet &excludedCurrenciesAll() const { return _excludedCurrenciesAll; }

  /// Get a reference to the list of statically excluded currency codes to consider for withdrawals.
  const CurrencySet &excludedCurrenciesWithdrawal() const { return _excludedCurrenciesWithdrawal; }

  /// Apply the general maker fee defined for this exchange on given MonetaryAmount.
  /// In other words, convert a gross amount into a net amount with maker fees
  MonetaryAmount applyFee(MonetaryAmount m, FeeType feeType) const {
    return m * (feeType == FeeType::kMaker ? _generalMakerRatio : _generalTakerRatio);
  }

  /// Get the minimum time between two public api queries
  Clock::duration minPublicQueryDelay() const { return _minPublicQueryDelay; }

  /// Get the minimum time between two public api queries
  Clock::duration minPrivateQueryDelay() const { return _minPrivateQueryDelay; }

  bool validateDepositAddressesInFile() const { return _validateDepositAddressesInFile; }

 private:
  CurrencySet _excludedCurrenciesAll;         // Currencies will be completely ignored by the exchange
  CurrencySet _excludedCurrenciesWithdrawal;  // Currencies unavailable for withdrawals
  Clock::duration _minPublicQueryDelay, _minPrivateQueryDelay;
  MonetaryAmount _generalMakerRatio;
  MonetaryAmount _generalTakerRatio;
  bool _validateDepositAddressesInFile;
};
}  // namespace cct
