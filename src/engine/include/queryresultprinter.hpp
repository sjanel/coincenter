#pragma once

#include <memory>
#include <optional>
#include <ostream>
#include <span>

#include "apioutputtype.hpp"
#include "cct_log.hpp"
#include "coincentercommandtype.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
#include "file.hpp"
#include "logginginfo.hpp"
#include "market.hpp"
#include "ordersconstraints.hpp"
#include "queryresulttypes.hpp"
#include "simpletable.hpp"
#include "time-window.hpp"
#include "withdrawsconstraints.hpp"
#include "write-json.hpp"

namespace cct {

class DeliveredWithdrawInfo;
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
                CoincenterCommandType::Trade);
  }

  void printBuyTrades(const TradeResultPerExchange &tradeResultPerExchange, MonetaryAmount endAmount,
                      const TradeOptions &tradeOptions) const {
    printTrades(tradeResultPerExchange, endAmount, false, CurrencyCode(), tradeOptions, CoincenterCommandType::Buy);
  }

  void printSellTrades(const TradeResultPerExchange &tradeResultPerExchange, MonetaryAmount startAmount,
                       bool isPercentageTrade, const TradeOptions &tradeOptions) const {
    printTrades(tradeResultPerExchange, startAmount, isPercentageTrade, CurrencyCode(), tradeOptions,
                CoincenterCommandType::Sell);
  }

  void printClosedOrders(const ClosedOrdersPerExchange &closedOrdersPerExchange,
                         const OrdersConstraints &ordersConstraints = OrdersConstraints{}) const;

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

  void printConversion(std::span<const MonetaryAmount> startAmountPerExchangePos, CurrencyCode targetCurrencyCode,
                       const MonetaryAmountPerExchange &conversionPerExchange) const;

  void printConversionPath(Market mk, const ConversionPathPerExchange &conversionPathsPerExchange) const;

  void printWithdrawFees(const MonetaryAmountByCurrencySetPerExchange &withdrawFeesPerExchange,
                         CurrencyCode currencyCode) const;

  void printLast24hTradedVolume(Market mk, const MonetaryAmountPerExchange &tradedVolumePerExchange) const;

  void printLastTrades(Market mk, std::optional<int> nbLastTrades,
                       const TradesPerExchange &lastTradesPerExchange) const;

  void printLastPrice(Market mk, const MonetaryAmountPerExchange &pricePerExchange) const;

  void printWithdraw(const DeliveredWithdrawInfoWithExchanges &deliveredWithdrawInfoWithExchanges,
                     bool isPercentageWithdraw, const WithdrawOptions &withdrawOptions) const;

  void printDustSweeper(
      const TradedAmountsVectorWithFinalAmountPerExchange &tradedAmountsVectorWithFinalAmountPerExchange,
      CurrencyCode currencyCode) const;

  void printMarketsForReplay(TimeWindow timeWindow,
                             const MarketTimestampSetsPerExchange &marketTimestampSetsPerExchange);

  void printMarketTradingResults(TimeWindow inputTimeWindow, const ReplayResults &replayResults,
                                 CoincenterCommandType commandType) const;

 private:
  void printTrades(const TradeResultPerExchange &tradeResultPerExchange, MonetaryAmount amount, bool isPercentageTrade,
                   CurrencyCode toCurrency, const TradeOptions &tradeOptions, CoincenterCommandType commandType) const;

  void printTable(const SimpleTable &table) const;

  void printJson(const auto &jsonObj) const {
    if (_pOs != nullptr) {
      *_pOs << WriteMiniJsonOrThrow(jsonObj) << '\n';
    } else {
      _outputLogger->info(WriteMiniJsonOrThrow(jsonObj));
    }
  }

  void logActivity(CoincenterCommandType commandType, const auto &jsonObj, bool isSimulationMode = false) const {
    if (_loggingInfo.isCommandTypeTracked(commandType) &&
        (!isSimulationMode || _loggingInfo.alsoLogActivityForSimulatedCommands())) {
      File activityFile = _loggingInfo.getActivityFile();
      activityFile.write(WriteMiniJsonOrThrow(jsonObj), Writer::Mode::Append);
    }
  }

  const LoggingInfo &_loggingInfo;
  std::ostream *_pOs = nullptr;
  std::shared_ptr<log::logger> _outputLogger;
  ApiOutputType _apiOutputType;
};

}  // namespace cct
