#pragma once

#include <optional>
#include <span>

#include "apikeysprovider.hpp"
#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "exchangepool.hpp"
#include "exchangesorchestrator.hpp"
#include "fiatconverter.hpp"
#include "market-trader-engine.hpp"
#include "market.hpp"
#include "metricsexporter.hpp"
#include "ordersconstraints.hpp"
#include "queryresultprinter.hpp"
#include "queryresulttypes.hpp"
#include "replay-options.hpp"
#include "transferablecommandresult.hpp"

namespace cct {

class AbstractMarketTraderFactory;
class CoincenterCommand;
class CoincenterCommands;
class TradeOptions;
class WithdrawOptions;

class Coincenter {
 public:
  using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

  Coincenter(const CoincenterInfo &coincenterInfo, const ExchangeSecretsInfo &exchangeSecretsInfo);

  /// Launch given commands and return the number of processed commands.
  int process(const CoincenterCommands &coincenterCommands);

  ExchangeHealthCheckStatus healthCheck(ExchangeNameSpan exchangeNames);

  /// Retrieve all tradable currencies for given selected public exchanges, or all if empty.
  CurrenciesPerExchange getCurrenciesPerExchange(ExchangeNameSpan exchangeNames);

  /// Retrieve the markets for given selected public exchanges (or all if empty span) matching given currencies.
  /// Currencies are both optional and may be neutral. A market matches any neutral currency.
  MarketsPerExchange getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2, ExchangeNameSpan exchangeNames);

  /// Retrieve ticker information for given selected public exchanges, or all if empty.
  ExchangeTickerMaps getTickerInformation(ExchangeNameSpan exchangeNames);

  /// Retrieve market order book of market for given exchanges
  /// Also adds the conversion rate of each Exchange bundled with the market order book.
  MarketOrderBookConversionRates getMarketOrderBooks(Market mk, ExchangeNameSpan exchangeNames,
                                                     CurrencyCode equiCurrencyCode,
                                                     std::optional<int> depth = std::nullopt);

  /// Query market data without returning it.
  /// This method is especially useful for serialization and metric exports.
  void queryMarketDataPerExchange(std::span<const Market> marketPerPublicExchange);

  /// Retrieve the last 24h traded volume for exchanges supporting given market.
  MonetaryAmountPerExchange getLast24hTradedVolumePerExchange(Market mk, ExchangeNameSpan exchangeNames);

  /// Retrieve the last trades for each queried exchange
  TradesPerExchange getLastTradesPerExchange(Market mk, ExchangeNameSpan exchangeNames, std::optional<int> depth);

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

  /// Get deposit information for given accounts
  WalletPerExchange getDepositInfo(std::span<const ExchangeName> privateExchangeNames, CurrencyCode depositCurrency);

  /// Get closed orders on given list of exchanges following given order constraints
  ClosedOrdersPerExchange getClosedOrders(std::span<const ExchangeName> privateExchangeNames,
                                          const OrdersConstraints &closedOrdersConstraints);

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

  /// Returns given amount converted into target currency code for given exchanges, when possible.
  MonetaryAmountPerExchange getConversion(MonetaryAmount amount, CurrencyCode targetCurrencyCode,
                                          ExchangeNameSpan exchangeNames);

  /// Returns given amount per exchange converted into target currency code for given exchanges, when possible.
  MonetaryAmountPerExchange getConversion(std::span<const MonetaryAmount> monetaryAmountPerExchangeToConvert,
                                          CurrencyCode targetCurrencyCode, ExchangeNameSpan exchangeNames);

  /// Query the conversion paths for each public exchange requested
  ConversionPathPerExchange getConversionPaths(Market mk, ExchangeNameSpan exchangeNames);

  /// Get withdraw fees for all exchanges from given list (or all exchanges if list is empty)
  MonetaryAmountByCurrencySetPerExchange getWithdrawFees(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames);

  /// Trade a specified amount of a given currency into another one, using the market defined in the given exchanges.
  /// If no exchange name is given, it will attempt to trade given amount on all exchanges with the sufficient balance.
  /// If exactly one private exchange is given, balance will not be queried and trade will be launched without balance
  /// check.
  TradeResultPerExchange trade(MonetaryAmount startAmount, bool isPercentageTrade, CurrencyCode toCurrency,
                               std::span<const ExchangeName> privateExchangeNames, const TradeOptions &tradeOptions);

  TradeResultPerExchange smartBuy(MonetaryAmount endAmount, std::span<const ExchangeName> privateExchangeNames,
                                  const TradeOptions &tradeOptions);

  TradeResultPerExchange smartSell(MonetaryAmount startAmount, bool isPercentageTrade,
                                   std::span<const ExchangeName> privateExchangeNames,
                                   const TradeOptions &tradeOptions);

  /// Single withdraw of 'grossAmount' from 'fromExchangeName' to 'toExchangeName'
  DeliveredWithdrawInfoWithExchanges withdraw(MonetaryAmount grossAmount, bool isPercentageWithdraw,
                                              const ExchangeName &fromPrivateExchangeName,
                                              const ExchangeName &toPrivateExchangeName,
                                              const WithdrawOptions &withdrawOptions);

  /// Retrieves the markets available for replay for exchanges selection that has some data during the last
  /// 'replayDuration' time (so within the time frame [now - replayDuration, now])
  MarketTimestampSetsPerExchange getMarketsAvailableForReplay(const ReplayOptions &replayOptions,
                                                              ExchangeNameSpan exchangeNames);

  /// Replay all markets for exchanges selection that has some data during the last
  /// 'replayDuration' time (so within the time frame [now - replayDuration, now])
  void replay(const AbstractMarketTraderFactory &marketTraderFactory, const ReplayOptions &replayOptions, Market market,
              ExchangeNameSpan exchangeNames);

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
  TransferableCommandResultVector processGroupedCommands(
      std::span<const CoincenterCommand> groupedCommands,
      std::span<const TransferableCommandResult> previousTransferableResults);

  using MarketTraderEngineVector = FixedCapacityVector<MarketTraderEngine, kNbSupportedExchanges>;

  void replayAlgorithm(const AbstractMarketTraderFactory &marketTraderFactory, std::string_view algorithmName,
                       const ReplayOptions &replayOptions, std::span<MarketTraderEngine> marketTraderEngines,
                       const PublicExchangeNameVector &exchangesWithThisMarketData);

  // TODO: may be moved somewhere else?
  MarketTraderEngineVector createMarketTraderEngines(const ReplayOptions &replayOptions, Market market,
                                                     PublicExchangeNameVector &exchangesWithThisMarketData);

  MarketTradeRangeStatsPerExchange tradingProcess(const ReplayOptions &replayOptions,
                                                  std::span<MarketTraderEngine> marketTraderEngines,
                                                  ExchangeNameSpan exchangesWithThisMarketData);

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
