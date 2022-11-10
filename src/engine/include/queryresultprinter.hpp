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

namespace cct {
class SimpleTable;
class TradeOptions;
class WithdrawInfo;

class QueryResultPrinter {
 public:
  /// @brief  Creates a QueryResultPrinter that will output result in the output logger.
  explicit QueryResultPrinter(ApiOutputType apiOutputType);

  /// @brief  Creates a QueryResultPrinter that will output result in given ostream
  QueryResultPrinter(std::ostream &os, ApiOutputType apiOutputType);

  void printMarkets(CurrencyCode cur1, CurrencyCode cur2, const MarketsPerExchange &marketsPerExchange) const;

  void printMarketOrderBooks(Market m, CurrencyCode equiCurrencyCode, std::optional<int> depth,
                             const MarketOrderBookConversionRates &marketOrderBooksConversionRates) const;

  void printTickerInformation(const ExchangeTickerMaps &exchangeTickerMaps) const;

  void printBalance(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency) const;

  void printDepositInfo(CurrencyCode depositCurrencyCode, const WalletPerExchange &walletPerExchange) const;

  void printTrades(const TradedAmountsPerExchange &tradedAmountsPerExchange, MonetaryAmount startAmount,
                   bool isPercentageTrade, CurrencyCode toCurrency, const TradeOptions &tradeOptions) const {
    printTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, toCurrency, tradeOptions,
                CoincenterCommandType::kTrade);
  }

  void printBuyTrades(const TradedAmountsPerExchange &tradedAmountsPerExchange, MonetaryAmount endAmount,
                      const TradeOptions &tradeOptions) const {
    printTrades(tradedAmountsPerExchange, endAmount, false, CurrencyCode(), tradeOptions, CoincenterCommandType::kBuy);
  }

  void printSellTrades(const TradedAmountsPerExchange &tradedAmountsPerExchange, MonetaryAmount startAmount,
                       bool isPercentageTrade, const TradeOptions &tradeOptions) const {
    printTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, CurrencyCode(), tradeOptions,
                CoincenterCommandType::kSell);
  }

  void printOpenedOrders(const OpenedOrdersPerExchange &openedOrdersPerExchange,
                         const OrdersConstraints &ordersConstraints) const;

  void printCancelledOrders(const NbCancelledOrdersPerExchange &nbCancelledOrdersPerExchange,
                            const OrdersConstraints &ordersConstraints) const;

  void printRecentDeposits(const DepositsPerExchange &depositsPerExchange,
                           const DepositsConstraints &depositsConstraints) const;

  void printConversionPath(Market m, const ConversionPathPerExchange &conversionPathsPerExchange) const;

  void printWithdrawFees(const MonetaryAmountPerExchange &withdrawFeePerExchange, CurrencyCode cur) const;

  void printLast24hTradedVolume(Market m, const MonetaryAmountPerExchange &tradedVolumePerExchange) const;

  void printLastTrades(Market m, int nbLastTrades, const LastTradesPerExchange &lastTradesPerExchange) const;

  void printLastPrice(Market m, const MonetaryAmountPerExchange &pricePerExchange) const;

  void printWithdraw(const WithdrawInfo &withdrawInfo, MonetaryAmount grossAmount, bool isPercentageWithdraw,
                     const ExchangeName &fromPrivateExchangeName, const ExchangeName &toPrivateExchangeName) const;

  void printDustSweeper(
      const TradedAmountsVectorWithFinalAmountPerExchange &tradedAmountsVectorWithFinalAmountPerExchange,
      CurrencyCode currencyCode) const;

 private:
  void printTrades(const TradedAmountsPerExchange &tradedAmountsPerExchange, MonetaryAmount amount,
                   bool isPercentageTrade, CurrencyCode toCurrency, const TradeOptions &tradeOptions,
                   CoincenterCommandType commandType) const;

  void printTable(const SimpleTable &t) const;

  void printJson(json &&in, json &&out) const;

  std::ostream *_pOs = nullptr;
  std::shared_ptr<log::logger> _outputLogger;
  ApiOutputType _apiOutputType;
};

}  // namespace cct
