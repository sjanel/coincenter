#pragma once

#include <span>
#include <string_view>

#include "apiquerytypeenum.hpp"
#include "cct_flatset.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {
class ExchangeInfo {
 public:
  using CurrencySet = FlatSet<CurrencyCode>;

  enum struct FeeType { kMaker, kTaker };

  struct APIUpdateFrequencies {
    Duration freq[api::kQueryTypeMax];
  };

  ExchangeInfo(std::string_view exchangeNameStr, std::string_view makerStr, std::string_view takerStr,
               std::span<const CurrencyCode> excludedAllCurrencies,
               std::span<const CurrencyCode> excludedCurrenciesWithdraw,
               const APIUpdateFrequencies &apiUpdateFrequencies, int minPublicQueryDelayMs, int minPrivateQueryDelayMs,
               bool validateDepositAddressesInFile, bool placeSimulateRealOrder);

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

  const APIUpdateFrequencies &apiUpdateFrequencies() const { return _apiUpdateFrequencies; }

  Duration getAPICallUpdateFrequency(api::QueryType apiCallType) const {
    return _apiUpdateFrequencies.freq[apiCallType];
  }

  /// Get the minimum time between two public api queries
  Duration minPublicQueryDelay() const { return _minPublicQueryDelay; }

  /// Get the minimum time between two public api queries
  Duration minPrivateQueryDelay() const { return _minPrivateQueryDelay; }

  bool validateDepositAddressesInFile() const { return _validateDepositAddressesInFile; }

  // In simulation mode for trade, for exchanges which do not have a simulation parameter, place a real order.
  // This real order price will have a limit price such that it should never be matched (if it is matched, lucky you!):
  // - Minimum for a buy (for instance, 1 USD for BTC)
  // - Maximum for a sell
  bool placeSimulateRealOrder() const { return _placeSimulateRealOrder; }

 private:
  CurrencySet _excludedCurrenciesAll;         // Currencies will be completely ignored by the exchange
  CurrencySet _excludedCurrenciesWithdrawal;  // Currencies unavailable for withdrawals
  APIUpdateFrequencies _apiUpdateFrequencies;
  Duration _minPublicQueryDelay, _minPrivateQueryDelay;
  MonetaryAmount _generalMakerRatio;
  MonetaryAmount _generalTakerRatio;
  bool _validateDepositAddressesInFile;
  bool _placeSimulateRealOrder;
};
}  // namespace cct
