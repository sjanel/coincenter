#pragma once

#include <memory>
#include <optional>
#include <ostream>

#include "apioutputtype.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincentercommandtype.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
#include "market.hpp"
#include "ordersconstraints.hpp"
#include "queryresulttypes.hpp"
#include "simpletable.hpp"
#include "withdrawsconstraints.hpp"

namespace cct {

class DeliveredWithdrawInfo;
class LoggingInfo;
class TradeOptions;
class WithdrawOptions;

class QueryResultPrinter {
 public:
  /// @brief Creates a QueryResultPrinter that will output result in the output logger.
  QueryResultPrinter(ApiOutputType apiOutputType, const LoggingInfo &loggingInfo);

  /// @brief Creates a QueryResultPrinter that will output result in given ostream
  QueryResultPrinter(std::ostream &os, ApiOutputType apiOutputType, const LoggingInfo &loggingInfo);

  void printHealthCheck(const ExchangeHealthCheckStatus &healthCheckPerExchange) const;

  void printCurrencies(const CurrenciesPerExchange &currenciesPerExchange) const;

  void printMarkets(CurrencyCode cur1, CurrencyCode cur2, const MarketsPerExchange &marketsPerExchange,
                    CoincenterCommandType coincenterCommandType) const;

  void printMarketOrderBooks(Market mk, CurrencyCode equiCurrencyCode, std::optional<int> depth,
                             const MarketOrderBookConversionRates &marketOrderBooksConversionRates) const;

  void printTickerInformation(const ExchangeTickerMaps &exchangeTickerMaps) const;

  void printBalance(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency) const;

  void printDepositInfo(CurrencyCode depositCurrencyCode, const WalletPerExchange &walletPerExchange) const;

  void printTrades(const TradeResultPerExchange &tradeResultPerExchange, MonetaryAmount startAmount,
                   bool isPercentageTrade, CurrencyCode toCurrency, const TradeOptions &tradeOptions) const {
    printTrades(tradeResultPerExchange, startAmount, isPercentageTrade, toCurrency, tradeOptions,
                CoincenterCommandType::kTrade);
  }

  void printBuyTrades(const TradeResultPerExchange &tradeResultPerExchange, MonetaryAmount endAmount,
                      const TradeOptions &tradeOptions) const {
    printTrades(tradeResultPerExchange, endAmount, false, CurrencyCode(), tradeOptions, CoincenterCommandType::kBuy);
  }

  void printSellTrades(const TradeResultPerExchange &tradeResultPerExchange, MonetaryAmount startAmount,
                       bool isPercentageTrade, const TradeOptions &tradeOptions) const {
    printTrades(tradeResultPerExchange, startAmount, isPercentageTrade, CurrencyCode(), tradeOptions,
                CoincenterCommandType::kSell);
  }

  void printClosedOrders(const ClosedOrdersPerExchange &closedOrdersPerExchange,
                         const OrdersConstraints &ordersConstraints) const;

  void printOpenedOrders(const OpenedOrdersPerExchange &openedOrdersPerExchange,
                         const OrdersConstraints &ordersConstraints) const;

  void printCancelledOrders(const NbCancelledOrdersPerExchange &nbCancelledOrdersPerExchange,
                            const OrdersConstraints &ordersConstraints) const;

  void printRecentDeposits(const DepositsPerExchange &depositsPerExchange,
                           const DepositsConstraints &depositsConstraints) const;

  void printRecentWithdraws(const WithdrawsPerExchange &withdrawsPerExchange,
                            const WithdrawsConstraints &withdrawsConstraints) const;

  void printConversion(MonetaryAmount amount, CurrencyCode targetCurrencyCode,
                       const MonetaryAmountPerExchange &conversionPerExchange) const;

  void printConversionPath(Market mk, const ConversionPathPerExchange &conversionPathsPerExchange) const;

  void printWithdrawFees(const MonetaryAmountByCurrencySetPerExchange &withdrawFeesPerExchange, CurrencyCode cur) const;

  void printLast24hTradedVolume(Market mk, const MonetaryAmountPerExchange &tradedVolumePerExchange) const;

  void printLastTrades(Market mk, int nbLastTrades, const TradesPerExchange &lastTradesPerExchange) const;

  void printLastPrice(Market mk, const MonetaryAmountPerExchange &pricePerExchange) const;

  void printWithdraw(const DeliveredWithdrawInfoWithExchanges &deliveredWithdrawInfoWithExchanges,
                     bool isPercentageWithdraw, const WithdrawOptions &withdrawOptions) const;

  void printDustSweeper(
      const TradedAmountsVectorWithFinalAmountPerExchange &tradedAmountsVectorWithFinalAmountPerExchange,
      CurrencyCode currencyCode) const;

 private:
  void printTrades(const TradeResultPerExchange &tradeResultPerExchange, MonetaryAmount amount, bool isPercentageTrade,
                   CurrencyCode toCurrency, const TradeOptions &tradeOptions, CoincenterCommandType commandType) const;

  void printTable(const SimpleTable &table) const;

  void printJson(const json &jsonData) const;

  void logActivity(CoincenterCommandType commandType, const json &data) const;

  const LoggingInfo &_loggingInfo;
  std::ostream *_pOs = nullptr;
  std::shared_ptr<log::logger> _outputLogger;
  ApiOutputType _apiOutputType;
};

}  // namespace cct
