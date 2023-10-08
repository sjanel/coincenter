#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "apikeysprovider.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
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
class WithdrawOptions;

class Coincenter {
 public:
  using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

  Coincenter(const CoincenterInfo &coincenterInfo, const ExchangeSecretsInfo &exchangeSecretsInfo);

  int process(const CoincenterCommands &coincenterCommands);

  ExchangeHealthCheckStatus healthCheck(ExchangeNameSpan exchangeNames);

  /// Retrieve the markets for given selected public exchanges, or all if empty.
  MarketsPerExchange getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2, ExchangeNameSpan exchangeNames);

  /// Retrieve ticker information for given selected public exchanges, or all if empty.
  ExchangeTickerMaps getTickerInformation(ExchangeNameSpan exchangeNames);

  /// Retrieve market order book of market for given exchanges
  /// Also adds the conversion rate of each Exchange bundled with the market order book.
  MarketOrderBookConversionRates getMarketOrderBooks(Market mk, ExchangeNameSpan exchangeNames,
                                                     CurrencyCode equiCurrencyCode,
                                                     std::optional<int> depth = std::nullopt);

  /// Retrieve the last 24h traded volume for exchanges supporting given market.
  MonetaryAmountPerExchange getLast24hTradedVolumePerExchange(Market mk, ExchangeNameSpan exchangeNames);

  /// Retrieve the last trades for each queried exchange
  LastTradesPerExchange getLastTradesPerExchange(Market mk, ExchangeNameSpan exchangeNames, int nbLastTrades);

  /// Retrieve the last price for exchanges supporting given market.
  MonetaryAmountPerExchange getLastPricePerExchange(Market mk, ExchangeNameSpan exchangeNames);

  /// Retrieve all matching Exchange references trading currency, at most one per platform.
  UniquePublicSelectedExchanges getExchangesTradingCurrency(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames,
                                                            bool shouldBeWithdrawable);

  /// Retrieve all matching Exchange references proposing market, at most one per platform.
  UniquePublicSelectedExchanges getExchangesTradingMarket(Market mk, ExchangeNameSpan exchangeNames);

  /// Query the private balance
  BalancePerExchange getBalance(std::span<const ExchangeName> privateExchangeNames,
                                const BalanceOptions &balanceOptions);

  WalletPerExchange getDepositInfo(std::span<const ExchangeName> privateExchangeNames, CurrencyCode depositCurrency);

  /// Get opened orders on given list of exchanges following given order constraints
  OpenedOrdersPerExchange getOpenedOrders(std::span<const ExchangeName> privateExchangeNames,
                                          const OrdersConstraints &openedOrdersConstraints);

  /// Cancel orders on given list of exchanges following given constraints
  NbCancelledOrdersPerExchange cancelOrders(std::span<const ExchangeName> privateExchangeNames,
                                            const OrdersConstraints &ordersConstraints);

  /// Get recent deposits on given list of exchanges following given constraints
  DepositsPerExchange getRecentDeposits(std::span<const ExchangeName> privateExchangeNames,
                                        const DepositsConstraints &depositsConstraints);

  /// Get recent withdraws on given list of exchanges following given constraints
  WithdrawsPerExchange getRecentWithdraws(std::span<const ExchangeName> privateExchangeNames,
                                          const WithdrawsConstraints &withdrawsConstraints);

  /// Attemps to sell all small amount of 'currencyCode' (dust) for given list of accounts.
  /// Dust threshold should be set first in the config file for the corresponding currency
  TradedAmountsVectorWithFinalAmountPerExchange dustSweeper(std::span<const ExchangeName> privateExchangeNames,
                                                            CurrencyCode currencyCode);

  /// Query the conversion paths for each public exchange requested
  ConversionPathPerExchange getConversionPaths(Market mk, ExchangeNameSpan exchangeNames);

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
  DeliveredWithdrawInfoWithExchanges withdraw(MonetaryAmount grossAmount, bool isPercentageWithdraw,
                                              const ExchangeName &fromPrivateExchangeName,
                                              const ExchangeName &toPrivateExchangeName,
                                              const WithdrawOptions &withdrawOptions);

  /// Dumps the content of all file caches in data directory to save cURL queries.
  void updateFileCaches() const;

  ExchangePool &exchangePool() { return _exchangePool; }
  const ExchangePool &exchangePool() const { return _exchangePool; }

  const CoincenterInfo &coincenterInfo() const { return _coincenterInfo; }

  api::CommonAPI &commonAPI() { return _commonAPI; }
  const api::CommonAPI &commonAPI() const { return _commonAPI; }

  FiatConverter &fiatConverter() { return _fiatConverter; }
  const FiatConverter &fiatConverter() const { return _fiatConverter; }

 private:
  void processCommand(const CoincenterCommand &cmd);

  const CoincenterInfo &_coincenterInfo;
  api::CommonAPI _commonAPI;
  FiatConverter _fiatConverter;
  api::APIKeysProvider _apiKeyProvider;

  MetricsExporter _metricsExporter;

  ExchangePool _exchangePool;
  ExchangesOrchestrator _exchangesOrchestrator;
  QueryResultPrinter _queryResultPrinter;
};
}  // namespace cct
