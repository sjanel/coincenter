#pragma once

#include <string_view>

#include "apiquerytypeenum.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencycodevector.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "timedef.hpp"

namespace cct {
class ExchangeInfo {
 public:
  enum struct FeeType { kMaker, kTaker };

  struct APIUpdateFrequencies {
    Duration freq[api::kQueryTypeMax];
  };

  ExchangeInfo(std::string_view exchangeNameStr, std::string_view makerStr, std::string_view takerStr,
               CurrencyCodeVector &&excludedAllCurrencies, CurrencyCodeVector &&excludedCurrenciesWithdraw,
               CurrencyCodeVector &&preferredPaymentCurrencies, MonetaryAmountByCurrencySet &&dustAmountsThreshold,
               const APIUpdateFrequencies &apiUpdateFrequencies, Duration publicAPIRate, Duration privateAPIRate,
               int dustSweeperMaxNbTrades, bool multiTradeAllowedByDefault, bool validateDepositAddressesInFile,
               bool placeSimulateRealOrder);

  /// Get a reference to the list of statically excluded currency codes to consider for the exchange,
  /// In both trading and withdrawal.
  const CurrencyCodeSet &excludedCurrenciesAll() const { return _excludedCurrenciesAll; }

  /// Get a reference to the list of statically excluded currency codes to consider for withdrawals.
  const CurrencyCodeSet &excludedCurrenciesWithdrawal() const { return _excludedCurrenciesWithdrawal; }

  /// Get a reference to the array of preferred payment currencies ordered by decreasing priority.
  const CurrencyCodeVector &preferredPaymentCurrencies() const { return _preferredPaymentCurrencies; }

  /// Get a reference to the set of monetary amounts representing the threshold for dust sweeper
  const MonetaryAmountByCurrencySet &dustAmountsThreshold() const { return _dustAmountsThreshold; }

  /// Maximum number of trades performed by the automatic dust sweeper process.
  /// A high value may have a higher chance of successfully sell to 0 the wanted currency,
  /// at the cost of more fees paid to the exchange.
  int dustSweeperMaxNbTrades() const { return _dustSweeperMaxNbTrades; }

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

  /// Get the minimum time between two private api queries
  Duration privateAPIRate() const { return _privateAPIRate; }

  bool validateDepositAddressesInFile() const { return _validateDepositAddressesInFile; }

  // In simulation mode for trade, for exchanges which do not have a simulation parameter, place a real order.
  // This real order price will have a limit price such that it should never be matched (if it is matched, lucky you!):
  // - Minimum for a buy (for instance, 1 USD for BTC)
  // - Maximum for a sell
  bool placeSimulateRealOrder() const { return _placeSimulateRealOrder; }

  bool multiTradeAllowedByDefault() const { return _multiTradeAllowedByDefault; }

 private:
  CurrencyCodeSet _excludedCurrenciesAll;             // Currencies will be completely ignored by the exchange
  CurrencyCodeSet _excludedCurrenciesWithdrawal;      // Currencies unavailable for withdrawals
  CurrencyCodeVector _preferredPaymentCurrencies;     // Ordered list of currencies available from smart trading.
  MonetaryAmountByCurrencySet _dustAmountsThreshold;  // Total amount in balance under one of these thresholds will be
                                                      // considered for dust sweeper
  APIUpdateFrequencies _apiUpdateFrequencies;
  Duration _publicAPIRate;
  Duration _privateAPIRate;
  MonetaryAmount _generalMakerRatio;
  MonetaryAmount _generalTakerRatio;
  int16_t _dustSweeperMaxNbTrades;  // max number of trades of a dust sweeper attempt per currency
  bool _multiTradeAllowedByDefault;
  bool _validateDepositAddressesInFile;
  bool _placeSimulateRealOrder;
};
}  // namespace cct
