#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "apikeysprovider.hpp"
#include "cct_smallvector.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "exchange.hpp"
#include "exchangename.hpp"
#include "exchangepool.hpp"
#include "exchangesorchestrator.hpp"
#include "fiatconverter.hpp"
#include "metricsexporter.hpp"
#include "monitoringinfo.hpp"
#include "ordersconstraints.hpp"
#include "queryresultprinter.hpp"
#include "queryresulttypes.hpp"
#include "tradedamounts.hpp"

namespace cct {

class CoincenterCommand;
class CoincenterCommands;
class TradeOptions;

class Coincenter {
 public:
  using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

  Coincenter(const CoincenterInfo &coincenterInfo, const ExchangeSecretsInfo &exchangeSecretsInfo);

  Coincenter(const Coincenter &) = delete;
  Coincenter(Coincenter &&) = delete;
  Coincenter &operator=(const Coincenter &) = delete;
  Coincenter &operator=(Coincenter &&) = delete;

  void process(const CoincenterCommands &opts);

  /// Retrieve the markets for given selected public exchanges, or all if empty.
  MarketsPerExchange getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2, ExchangeNameSpan exchangeNames);

  /// Retrieve ticker information for given selected public exchanges, or all if empty.
  ExchangeTickerMaps getTickerInformation(ExchangeNameSpan exchangeNames);

  /// Retrieve market order book of market for given exchanges
  /// Also adds the conversion rate of each Exchange bundled with the market order book.
  MarketOrderBookConversionRates getMarketOrderBooks(Market m, ExchangeNameSpan exchangeNames,
                                                     CurrencyCode equiCurrencyCode,
                                                     std::optional<int> depth = std::nullopt);

  /// Retrieve the last 24h traded volume for exchanges supporting given market.
  MonetaryAmountPerExchange getLast24hTradedVolumePerExchange(Market m, ExchangeNameSpan exchangeNames);

  /// Retrieve the last trades for each queried exchange
  LastTradesPerExchange getLastTradesPerExchange(Market m, ExchangeNameSpan exchangeNames, int nbLastTrades);

  /// Retrieve the last price for exchanges supporting given market.
  MonetaryAmountPerExchange getLastPricePerExchange(Market m, ExchangeNameSpan exchangeNames);

  /// Retrieve all matching Exchange references trading currency, at most one per platform.
  UniquePublicSelectedExchanges getExchangesTradingCurrency(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames,
                                                            bool shouldBeWithdrawable);

  /// Retrieve all matching Exchange references proposing market, at most one per platform.
  UniquePublicSelectedExchanges getExchangesTradingMarket(Market m, ExchangeNameSpan exchangeNames);

  /// Query the private balance
  BalancePerExchange getBalance(std::span<const ExchangeName> privateExchangeNames,
                                CurrencyCode equiCurrency = CurrencyCode());

  WalletPerExchange getDepositInfo(std::span<const ExchangeName> privateExchangeNames, CurrencyCode depositCurrency);

  /// Get opened orders on given list of exchanges following given order constraints
  OpenedOrdersPerExchange getOpenedOrders(std::span<const ExchangeName> privateExchangeNames,
                                          const OrdersConstraints &openedOrdersConstraints);

  /// Cancel orders on given list of exchanges following given constraints
  NbCancelledOrdersPerExchange cancelOrders(std::span<const ExchangeName> privateExchangeNames,
                                            const OrdersConstraints &ordersConstraints);

  /// Query the conversion paths for each public exchange requested
  ConversionPathPerExchange getConversionPaths(Market m, ExchangeNameSpan exchangeNames);

  /// Get withdraw fees for all exchanges from given list (or all exchanges if list is empty)
  MonetaryAmountPerExchange getWithdrawFees(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames);

  /// Trade a specified amount of a given currency into another one, using the market defined in the given exchanges.
  /// If no exchange name is given, it will attempt to trade given amount on all exchanges with the sufficient balance.
  /// If exactly one private exchange is given, balance will not be queried and trade will be launched without balance
  /// check.
  TradedAmountsPerExchange trade(MonetaryAmount startAmount, bool isPercentageTrade, CurrencyCode toCurrency,
                                 std::span<const ExchangeName> privateExchangeNames, const TradeOptions &tradeOptions);

  TradedAmountsPerExchange smartBuy(MonetaryAmount endAmount, std::span<const ExchangeName> privateExchangeNames,
                                    const TradeOptions &tradeOptions);

  TradedAmountsPerExchange smartSell(MonetaryAmount startAmount, bool isPercentageTrade,
                                     std::span<const ExchangeName> privateExchangeNames,
                                     const TradeOptions &tradeOptions);

  /// Single withdraw of 'grossAmount' from 'fromExchangeName' to 'toExchangeName'
  WithdrawInfo withdraw(MonetaryAmount grossAmount, bool isPercentageWithdraw,
                        const ExchangeName &fromPrivateExchangeName, const ExchangeName &toPrivateExchangeName);

  /// Dumps the content of all file caches in data directory to save cURL queries.
  void updateFileCaches() const;

  ExchangePool &exchangePool() { return _exchangePool; }
  const ExchangePool &exchangePool() const { return _exchangePool; }

  const CoincenterInfo &coincenterInfo() const { return _coincenterInfo; }

  api::CryptowatchAPI &cryptowatchAPI() { return _cryptowatchAPI; }
  const api::CryptowatchAPI &cryptowatchAPI() const { return _cryptowatchAPI; }

  FiatConverter &fiatConverter() { return _fiatConverter; }
  const FiatConverter &fiatConverter() const { return _fiatConverter; }

 private:
  void processCommand(const CoincenterCommand &cmd);

  CurlInitRAII _curlInitRAII;  // Should be first
  const CoincenterInfo &_coincenterInfo;
  api::CryptowatchAPI _cryptowatchAPI;
  FiatConverter _fiatConverter;
  api::APIKeysProvider _apiKeyProvider;

  MetricsExporter _metricsExporter;

  ExchangePool _exchangePool;
  ExchangesOrchestrator _exchangesOrchestrator;
  QueryResultPrinter _queryResultPrinter;
};
}  // namespace cct
