#pragma once

#include <string_view>

#include "apiquerytypeenum.hpp"
#include "cct_flatset.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {
class ExchangeInfo {
 public:
  using CurrencySet = FlatSet<CurrencyCode>;
  using CurrencyVector = vector<CurrencyCode>;

  enum struct FeeType { kMaker, kTaker };

  struct APIUpdateFrequencies {
    Duration freq[api::kQueryTypeMax];
  };

  ExchangeInfo(std::string_view exchangeNameStr, std::string_view makerStr, std::string_view takerStr,
               CurrencyVector &&excludedAllCurrencies, CurrencyVector &&excludedCurrenciesWithdraw,
               CurrencyVector &&preferredPaymentCurrencies, const APIUpdateFrequencies &apiUpdateFrequencies,
               Duration publicAPIRate, Duration privateAPIRate, bool multiTradeAllowedByDefault,
               bool validateDepositAddressesInFile, bool placeSimulateRealOrder);

  /// Get a reference to the list of statically excluded currency codes to consider for the exchange,
  /// In both trading and withdrawal.
  const CurrencySet &excludedCurrenciesAll() const { return _excludedCurrenciesAll; }

  /// Get a reference to the list of statically excluded currency codes to consider for withdrawals.
  const CurrencySet &excludedCurrenciesWithdrawal() const { return _excludedCurrenciesWithdrawal; }

  /// Get a reference to the array of preferred payment currencies ordered by decreasing priority.
  const CurrencyVector &preferredPaymentCurrencies() const { return _preferredPaymentCurrencies; }

  /// Apply the general maker fee defined for this exchange on given MonetaryAmount.
  /// In other words, convert a gross amount into a net amount with maker fees
  MonetaryAmount applyFee(MonetaryAmount m, FeeType feeType) const {
    return m * (feeType == FeeType::kMaker ? _generalMakerRatio : _generalTakerRatio);
  }

  MonetaryAmount getMakerFeeRatio() const { return _generalMakerRatio; }

  MonetaryAmount getTakerFeeRatio() const { return _generalTakerRatio; }

  const APIUpdateFrequencies &apiUpdateFrequencies() const { return _apiUpdateFrequencies; }

  Duration getAPICallUpdateFrequency(api::QueryType apiCallType) const {
    return _apiUpdateFrequencies.freq[apiCallType];
  }

  /// Get the minimum time between two public api queries
  Duration publicAPIRate() const { return _publicAPIRate; }

  /// Get the minimum time between two public api queries
  Duration privateAPIRate() const { return _privateAPIRate; }

  bool validateDepositAddressesInFile() const { return _validateDepositAddressesInFile; }

  // In simulation mode for trade, for exchanges which do not have a simulation parameter, place a real order.
  // This real order price will have a limit price such that it should never be matched (if it is matched, lucky you!):
  // - Minimum for a buy (for instance, 1 USD for BTC)
  // - Maximum for a sell
  bool placeSimulateRealOrder() const { return _placeSimulateRealOrder; }

  bool multiTradeAllowedByDefault() const { return _multiTradeAllowedByDefault; }

 private:
  CurrencySet _excludedCurrenciesAll;          // Currencies will be completely ignored by the exchange
  CurrencySet _excludedCurrenciesWithdrawal;   // Currencies unavailable for withdrawals
  CurrencyVector _preferredPaymentCurrencies;  // Ordered list of currencies available from smart trading.
  APIUpdateFrequencies _apiUpdateFrequencies;
  Duration _publicAPIRate;
  Duration _privateAPIRate;
  MonetaryAmount _generalMakerRatio;
  MonetaryAmount _generalTakerRatio;
  bool _multiTradeAllowedByDefault;
  bool _validateDepositAddressesInFile;
  bool _placeSimulateRealOrder;
};
}  // namespace cct
