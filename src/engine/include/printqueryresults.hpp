#pragma once

#include "currencycode.hpp"
#include "market.hpp"
#include "queryresulttypes.hpp"

namespace cct {
class QueryResultPrinter {
 public:
  explicit QueryResultPrinter(bool doPrint) : _doPrint(doPrint) {}

  void printMarkets(CurrencyCode cur1, CurrencyCode cur2, const MarketsPerExchange &marketsPerExchange);

  void printMarketOrderBooks(const MarketOrderBookConversionRates &marketOrderBooksConversionRates);

  void printTickerInformation(const ExchangeTickerMaps &exchangeTickerMaps);

  void printBalance(const BalancePerExchange &balancePerExchange);

  void printDepositInfo(CurrencyCode depositCurrencyCode, const WalletPerExchange &walletPerExchange);

  void printConversionPath(Market m, const ConversionPathPerExchange &conversionPathsPerExchange);

  void printWithdrawFees(const WithdrawFeePerExchange &withdrawFeePerExchange);

  void printLast24hTradedVolume(Market m, const MonetaryAmountPerExchange &tradedVolumePerExchange);

  void printLastTrades(Market m, const LastTradesPerExchange &lastTradesPerExchange);

  void printLastPrice(Market m, const MonetaryAmountPerExchange &pricePerExchange);

 private:
  bool _doPrint;
};

}  // namespace cct