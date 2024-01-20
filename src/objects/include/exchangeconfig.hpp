#pragma once

#include <string_view>

#include "apiquerytypeenum.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencycodevector.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "timedef.hpp"
#include "tradeconfig.hpp"

namespace cct {
class ExchangeConfig {
 public:
  enum struct FeeType { kMaker, kTaker };

  struct APIUpdateFrequencies {
    Duration freq[api::kQueryTypeMax];
  };

  ExchangeConfig(std::string_view exchangeNameStr, std::string_view makerStr, std::string_view takerStr,
                 CurrencyCodeVector &&excludedAllCurrencies, CurrencyCodeVector &&excludedCurrenciesWithdraw,
                 CurrencyCodeVector &&preferredPaymentCurrencies, MonetaryAmountByCurrencySet &&dustAmountsThreshold,
                 const APIUpdateFrequencies &apiUpdateFrequencies, Duration publicAPIRate, Duration privateAPIRate,
                 std::string_view acceptEncoding, int dustSweeperMaxNbTrades,
                 log::level::level_enum requestsCallLogLevel, log::level::level_enum requestsAnswerLogLevel,
                 bool multiTradeAllowedByDefault, bool validateDepositAddressesInFile, bool placeSimulateRealOrder,
                 bool validateApiKey, TradeConfig tradeConfig);

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

  // Log level for request calls
  log::level::level_enum requestsCallLogLevel() const { return LevelFromPos(_requestsCallLogLevel); }

  // Log level for requests replies, should it be json, or any other type
  log::level::level_enum requestsAnswerLogLevel() const { return LevelFromPos(_requestsAnswerLogLevel); }

  /// Apply the general maker fee defined for this exchange on given MonetaryAmount.
  /// In other words, convert a gross amount into a net amount with maker fees
  MonetaryAmount applyFee(MonetaryAmount mk, FeeType feeType) const {
    return mk * (feeType == FeeType::kMaker ? _generalMakerRatio : _generalTakerRatio);
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

  /// Get the comma separated list of accepted encodings sent to queries as header 'Accept-Encoding' (can be empty to
  /// remove the header)
  std::string_view acceptEncoding() const { return _acceptEncoding; }

  bool validateDepositAddressesInFile() const { return _validateDepositAddressesInFile; }

  // Returns true if we need to validate API key at each ExchangePrivate object construction.
  // Benefit is that in the case an API key is detected as invalid, program will evict the corresponding exchange
  // for the next commands including it.
  bool shouldValidateApiKey() const { return _validateApiKey; }

  // In simulation mode for trade, for exchanges which do not have a simulation parameter, place a real order.
  // This real order price will have a limit price such that it should never be matched (if it is matched, lucky you!):
  // - Minimum for a buy (for instance, 1 USD for BTC)
  // - Maximum for a sell
  bool placeSimulateRealOrder() const { return _placeSimulateRealOrder; }

  bool multiTradeAllowedByDefault() const { return _multiTradeAllowedByDefault; }

  const TradeConfig &tradeConfig() const { return _tradeConfig; }

 private:
  CurrencyCodeSet _excludedCurrenciesAll;             // Currencies will be completely ignored by the exchange
  CurrencyCodeSet _excludedCurrenciesWithdrawal;      // Currencies unavailable for withdrawals
  CurrencyCodeVector _preferredPaymentCurrencies;     // Ordered list of currencies available from smart trading.
  MonetaryAmountByCurrencySet _dustAmountsThreshold;  // Total amount in balance under one of these thresholds will be
                                                      // considered for dust sweeper
  APIUpdateFrequencies _apiUpdateFrequencies;
  Duration _publicAPIRate;
  Duration _privateAPIRate;
  string _acceptEncoding;
  MonetaryAmount _generalMakerRatio;
  MonetaryAmount _generalTakerRatio;
  TradeConfig _tradeConfig;
  int16_t _dustSweeperMaxNbTrades;  // max number of trades of a dust sweeper attempt per currency
  int8_t _requestsCallLogLevel;
  int8_t _requestsAnswerLogLevel;
  bool _multiTradeAllowedByDefault;
  bool _validateDepositAddressesInFile;
  bool _placeSimulateRealOrder;
  bool _validateApiKey;
};
}  // namespace cct
