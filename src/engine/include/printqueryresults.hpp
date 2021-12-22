#pragma once

#include "currencycode.hpp"
#include "market.hpp"
#include "queryresulttypes.hpp"

namespace cct {
class QueryResultPrinter {
 public:
  explicit QueryResultPrinter(bool doPrint) : _doPrint(doPrint) {}

  void printMarkets(CurrencyCode cur1, CurrencyCode cur2, const MarketsPerExchange &marketsPerExchange) const;

  void printMarketOrderBooks(const MarketOrderBookConversionRates &marketOrderBooksConversionRates) const;

  void printTickerInformation(const ExchangeTickerMaps &exchangeTickerMaps) const;

  void printBalance(const BalancePerExchange &balancePerExchange) const;

  void printDepositInfo(CurrencyCode depositCurrencyCode, const WalletPerExchange &walletPerExchange) const;

  void printOpenedOrders(const OpenedOrdersPerExchange &openedOrdersPerExchange) const;

  void printConversionPath(Market m, const ConversionPathPerExchange &conversionPathsPerExchange) const;

  void printWithdrawFees(const WithdrawFeePerExchange &withdrawFeePerExchange) const;

  void printLast24hTradedVolume(Market m, const MonetaryAmountPerExchange &tradedVolumePerExchange) const;

  void printLastTrades(Market m, const LastTradesPerExchange &lastTradesPerExchange) const;

  void printLastPrice(Market m, const MonetaryAmountPerExchange &pricePerExchange) const;

 private:
  bool _doPrint;
};

}  // namespace cct