#include "coincenter.hpp"

#include <algorithm>
#include <array>
#include <csignal>
#include <iterator>
#include <optional>
#include <span>
#include <thread>
#include <utility>

#include "balanceoptions.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "coincenter-commands-iterator.hpp"
#include "coincentercommand.hpp"
#include "coincentercommands.hpp"
#include "coincentercommandtype.hpp"
#include "coincenterinfo.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
#include "durationstring.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "exchangepublicapi.hpp"
#include "exchangeretriever.hpp"
#include "exchangesecretsinfo.hpp"
#include "market-trader-engine.hpp"
#include "market-trader-factory.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "queryresultprinter.hpp"
#include "queryresulttypes.hpp"
#include "replay-algorithm-name-iterator.hpp"
#include "replay-options.hpp"
#include "time-window.hpp"
#include "transferablecommandresult.hpp"
#include "withdrawsconstraints.hpp"

namespace cct {
namespace {
void FillTransferableCommandResults(const TradeResultPerExchange &tradeResultPerExchange,
                                    TransferableCommandResultVector &transferableResults) {
  for (const auto &[exchangePtr, tradeResult] : tradeResultPerExchange) {
    if (tradeResult.isComplete()) {
      transferableResults.emplace_back(exchangePtr->createExchangeName(), tradeResult.tradedAmounts().to);
    }
  }
}
}  // namespace

volatile sig_atomic_t g_signalStatus = 0;

// According to the standard, 'SignalHandler' function should have C linkage:
// https://en.cppreference.com/w/cpp/utility/program/signal
// Thus it's not possible to use a lambda and pass some
// objects to it. This is why for this rare occasion we will rely on a static variable. This solution has been inspired
// by: https://wiki.sei.cmu.edu/confluence/display/cplusplus/MSC54-CPP.+A+signal+handler+must+be+a+plain+old+function
extern "C" void SignalHandler(int sigNum) {
  log::warn("Signal {} received, will stop after current request", sigNum);
  g_signalStatus = sigNum;

  // Revert to standard signal handler (to allow for standard kill in case program does not react)
  std::signal(sigNum, SIG_DFL);
}

using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

Coincenter::Coincenter(const CoincenterInfo &coincenterInfo, const ExchangeSecretsInfo &exchangeSecretsInfo)
    : _coincenterInfo(coincenterInfo),
      _commonAPI(coincenterInfo),
      _fiatConverter(coincenterInfo, coincenterInfo.fiatConversionQueryRate()),
      _apiKeyProvider(coincenterInfo.dataDir(), exchangeSecretsInfo, coincenterInfo.getRunMode()),
      _metricsExporter(coincenterInfo.metricGatewayPtr()),
      _exchangePool(coincenterInfo, _fiatConverter, _commonAPI, _apiKeyProvider),
      _exchangesOrchestrator(coincenterInfo.requestsConfig(), _exchangePool.exchanges()),
      _queryResultPrinter(coincenterInfo.apiOutputType(), _coincenterInfo.loggingInfo()) {
  // Register the signal handler to gracefully shutdown the main loop for repeated requests.
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);
}

int Coincenter::process(const CoincenterCommands &coincenterCommands) {
  const auto commands = coincenterCommands.commands();
  const int nbRepeats = commands.empty() ? 0 : coincenterCommands.repeats();
  const auto repeatTime = coincenterCommands.repeatTime();

  int nbCommandsProcessed = 0;
  TimePoint lastCommandTime;
  for (int repeatPos = 0; repeatPos != nbRepeats && g_signalStatus == 0; ++repeatPos) {
    const auto earliestTimeNextCommand = lastCommandTime + repeatTime;
    lastCommandTime = Clock::now();

    if (earliestTimeNextCommand > lastCommandTime) {
      const auto waitingDuration = earliestTimeNextCommand - lastCommandTime;

      lastCommandTime += waitingDuration;

      log::debug("Sleep for {} before next command", DurationToString(waitingDuration));
      std::this_thread::sleep_for(waitingDuration);
    }
    if (nbRepeats != 1) {
      if (nbRepeats == -1) {
        log::info("Process request {}", repeatPos + 1);
      } else {
        log::info("Process request {}/{}", repeatPos + 1, nbRepeats);
      }
    }
    TransferableCommandResultVector transferableResults;
    CoincenterCommandsIterator commandsIterator(commands);
    while (commandsIterator.hasNextCommandGroup()) {
      const auto groupedCommands = commandsIterator.nextCommandGroup();
      transferableResults = processGroupedCommands(groupedCommands, transferableResults);
      ++nbCommandsProcessed;
    }
  }
  return nbCommandsProcessed;
}

TransferableCommandResultVector Coincenter::processGroupedCommands(
    std::span<const CoincenterCommand> groupedCommands,
    std::span<const TransferableCommandResult> previousTransferableResults) {
  TransferableCommandResultVector transferableResults;
  const auto &firstCmd = groupedCommands.front();
  // All grouped commands have same type - logic to handle multiple commands in a group should be handled per use case
  switch (firstCmd.type()) {
    case CoincenterCommandType::kHealthCheck: {
      const auto healthCheckStatus = healthCheck(firstCmd.exchangeNames());
      _queryResultPrinter.printHealthCheck(healthCheckStatus);
      break;
    }
    case CoincenterCommandType::kCurrencies: {
      const auto currenciesPerExchange = getCurrenciesPerExchange(firstCmd.exchangeNames());
      _queryResultPrinter.printCurrencies(currenciesPerExchange);
      break;
    }
    case CoincenterCommandType::kMarkets: {
      const auto marketsPerExchange = getMarketsPerExchange(firstCmd.cur1(), firstCmd.cur2(), firstCmd.exchangeNames());
      _queryResultPrinter.printMarkets(firstCmd.cur1(), firstCmd.cur2(), marketsPerExchange, firstCmd.type());
      break;
    }
    case CoincenterCommandType::kConversion: {
      const auto conversionPerExchange = getConversion(firstCmd.amount(), firstCmd.cur1(), firstCmd.exchangeNames());
      _queryResultPrinter.printConversion(firstCmd.amount(), firstCmd.cur1(), conversionPerExchange);
      break;
    }
    case CoincenterCommandType::kConversionPath: {
      const auto conversionPathPerExchange = getConversionPaths(firstCmd.market(), firstCmd.exchangeNames());
      _queryResultPrinter.printConversionPath(firstCmd.market(), conversionPathPerExchange);
      break;
    }
    case CoincenterCommandType::kLastPrice: {
      const auto lastPricePerExchange = getLastPricePerExchange(firstCmd.market(), firstCmd.exchangeNames());
      _queryResultPrinter.printLastPrice(firstCmd.market(), lastPricePerExchange);
      break;
    }
    case CoincenterCommandType::kTicker: {
      const auto exchangeTickerMaps = getTickerInformation(firstCmd.exchangeNames());
      _queryResultPrinter.printTickerInformation(exchangeTickerMaps);
      break;
    }
    case CoincenterCommandType::kOrderbook: {
      const auto marketOrderBooksConversionRates =
          getMarketOrderBooks(firstCmd.market(), firstCmd.exchangeNames(), firstCmd.cur1(), firstCmd.optDepth());
      _queryResultPrinter.printMarketOrderBooks(firstCmd.market(), firstCmd.cur1(), firstCmd.optDepth(),
                                                marketOrderBooksConversionRates);
      break;
    }
    case CoincenterCommandType::kLastTrades: {
      const auto lastTradesPerExchange =
          getLastTradesPerExchange(firstCmd.market(), firstCmd.exchangeNames(), firstCmd.optDepth());
      _queryResultPrinter.printLastTrades(firstCmd.market(), firstCmd.optDepth(), lastTradesPerExchange);
      break;
    }
    case CoincenterCommandType::kLast24hTradedVolume: {
      const auto tradedVolumePerExchange =
          getLast24hTradedVolumePerExchange(firstCmd.market(), firstCmd.exchangeNames());
      _queryResultPrinter.printLast24hTradedVolume(firstCmd.market(), tradedVolumePerExchange);
      break;
    }
    case CoincenterCommandType::kWithdrawFees: {
      const auto withdrawFeesPerExchange = getWithdrawFees(firstCmd.cur1(), firstCmd.exchangeNames());
      _queryResultPrinter.printWithdrawFees(withdrawFeesPerExchange, firstCmd.cur1());
      break;
    }

    case CoincenterCommandType::kBalance: {
      const auto amountIncludePolicy = firstCmd.withBalanceInUse()
                                           ? BalanceOptions::AmountIncludePolicy::kWithBalanceInUse
                                           : BalanceOptions::AmountIncludePolicy::kOnlyAvailable;
      const BalanceOptions balanceOptions(amountIncludePolicy, firstCmd.cur1());
      const auto balancePerExchange = getBalance(firstCmd.exchangeNames(), balanceOptions);
      _queryResultPrinter.printBalance(balancePerExchange, firstCmd.cur1());
      break;
    }
    case CoincenterCommandType::kDepositInfo: {
      const auto walletPerExchange = getDepositInfo(firstCmd.exchangeNames(), firstCmd.cur1());
      _queryResultPrinter.printDepositInfo(firstCmd.cur1(), walletPerExchange);
      break;
    }
    case CoincenterCommandType::kOrdersClosed: {
      const auto closedOrdersPerExchange = getClosedOrders(firstCmd.exchangeNames(), firstCmd.ordersConstraints());
      _queryResultPrinter.printClosedOrders(closedOrdersPerExchange, firstCmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::kOrdersOpened: {
      const auto openedOrdersPerExchange = getOpenedOrders(firstCmd.exchangeNames(), firstCmd.ordersConstraints());
      _queryResultPrinter.printOpenedOrders(openedOrdersPerExchange, firstCmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::kOrdersCancel: {
      const auto nbCancelledOrdersPerExchange = cancelOrders(firstCmd.exchangeNames(), firstCmd.ordersConstraints());
      _queryResultPrinter.printCancelledOrders(nbCancelledOrdersPerExchange, firstCmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::kRecentDeposits: {
      const auto depositsPerExchange =
          getRecentDeposits(firstCmd.exchangeNames(), firstCmd.withdrawsOrDepositsConstraints());
      _queryResultPrinter.printRecentDeposits(depositsPerExchange, firstCmd.withdrawsOrDepositsConstraints());
      break;
    }
    case CoincenterCommandType::kRecentWithdraws: {
      const auto withdrawsPerExchange =
          getRecentWithdraws(firstCmd.exchangeNames(), firstCmd.withdrawsOrDepositsConstraints());
      _queryResultPrinter.printRecentWithdraws(withdrawsPerExchange, firstCmd.withdrawsOrDepositsConstraints());
      break;
    }
    case CoincenterCommandType::kTrade: {
      // 2 input styles are possible:
      //  - standard full information with an amount to trade, a destination currency and an optional list of exchanges
      //  where to trade
      //  - a currency - the destination one, and start amount and exchange(s) should come from previous command result
      auto [startAmount, exchangeNames] = ComputeTradeAmountAndExchanges(firstCmd, previousTransferableResults);
      if (startAmount.isDefault()) {
        break;
      }
      const auto tradeResultPerExchange =
          trade(startAmount, firstCmd.isPercentageAmount(), firstCmd.cur1(), exchangeNames, firstCmd.tradeOptions());
      _queryResultPrinter.printTrades(tradeResultPerExchange, startAmount, firstCmd.isPercentageAmount(),
                                      firstCmd.cur1(), firstCmd.tradeOptions());
      FillTransferableCommandResults(tradeResultPerExchange, transferableResults);
      break;
    }
    case CoincenterCommandType::kBuy: {
      const auto tradeResultPerExchange =
          smartBuy(firstCmd.amount(), firstCmd.exchangeNames(), firstCmd.tradeOptions());
      _queryResultPrinter.printBuyTrades(tradeResultPerExchange, firstCmd.amount(), firstCmd.tradeOptions());
      FillTransferableCommandResults(tradeResultPerExchange, transferableResults);
      break;
    }
    case CoincenterCommandType::kSell: {
      auto [startAmount, exchangeNames] = ComputeTradeAmountAndExchanges(firstCmd, previousTransferableResults);
      if (startAmount.isDefault()) {
        break;
      }
      const auto tradeResultPerExchange =
          smartSell(startAmount, firstCmd.isPercentageAmount(), exchangeNames, firstCmd.tradeOptions());
      _queryResultPrinter.printSellTrades(tradeResultPerExchange, firstCmd.amount(), firstCmd.isPercentageAmount(),
                                          firstCmd.tradeOptions());
      FillTransferableCommandResults(tradeResultPerExchange, transferableResults);
      break;
    }
    case CoincenterCommandType::kWithdrawApply: {
      const auto [grossAmount, exchangeName] = ComputeWithdrawAmount(firstCmd, previousTransferableResults);
      if (grossAmount.isDefault()) {
        break;
      }
      const auto deliveredWithdrawInfoWithExchanges =
          withdraw(grossAmount, firstCmd.isPercentageAmount(), exchangeName, firstCmd.exchangeNames().back(),
                   firstCmd.withdrawOptions());
      _queryResultPrinter.printWithdraw(deliveredWithdrawInfoWithExchanges, firstCmd.isPercentageAmount(),
                                        firstCmd.withdrawOptions());
      transferableResults.emplace_back(deliveredWithdrawInfoWithExchanges.first[1]->createExchangeName(),
                                       deliveredWithdrawInfoWithExchanges.second.receivedAmount());
      break;
    }
    case CoincenterCommandType::kDustSweeper: {
      _queryResultPrinter.printDustSweeper(dustSweeper(firstCmd.exchangeNames(), firstCmd.cur1()), firstCmd.cur1());
      break;
    }
    case CoincenterCommandType::kMarketData: {
      std::array<Market, kNbSupportedExchanges> marketPerPublicExchange;
      for (const auto &cmd : groupedCommands) {
        if (cmd.exchangeNames().empty()) {
          std::ranges::fill(marketPerPublicExchange, cmd.market());
        } else {
          for (const auto &exchangeName : cmd.exchangeNames()) {
            marketPerPublicExchange[exchangeName.publicExchangePos()] = cmd.market();
          }
        }
      }
      // No return value here, this command is made only for storing purposes.
      queryMarketDataPerExchange(marketPerPublicExchange);
      break;
    }
    case CoincenterCommandType::kReplay: {
      replay(firstCmd.replayOptions(), firstCmd.market(), firstCmd.exchangeNames());
      break;
    }
    case CoincenterCommandType::kReplayMarkets: {
      const auto marketsPerExchange = getMarketsAvailableForReplay(firstCmd.replayOptions(), firstCmd.exchangeNames());
      _queryResultPrinter.printMarkets(CurrencyCode{}, CurrencyCode{}, marketsPerExchange, firstCmd.type());
      break;
    }
    default:
      throw exception("Unknown command type");
  }
  return transferableResults;
}

ExchangeHealthCheckStatus Coincenter::healthCheck(ExchangeNameSpan exchangeNames) {
  const auto ret = _exchangesOrchestrator.healthCheck(exchangeNames);

  _metricsExporter.exportHealthCheckMetrics(ret);

  return ret;
}

ExchangeTickerMaps Coincenter::getTickerInformation(ExchangeNameSpan exchangeNames) {
  const auto ret = _exchangesOrchestrator.getTickerInformation(exchangeNames);

  _metricsExporter.exportTickerMetrics(ret);

  return ret;
}

MarketOrderBookConversionRates Coincenter::getMarketOrderBooks(Market mk, ExchangeNameSpan exchangeNames,
                                                               CurrencyCode equiCurrencyCode,
                                                               std::optional<int> depth) {
  const auto ret = _exchangesOrchestrator.getMarketOrderBooks(mk, exchangeNames, equiCurrencyCode, depth);

  _metricsExporter.exportOrderbookMetrics(ret);

  return ret;
}

void Coincenter::queryMarketDataPerExchange(std::span<const Market> marketPerPublicExchange) {
  ExchangeNames exchangeNames;

  int exchangePos{};
  for (Market market : marketPerPublicExchange) {
    if (market.isDefined()) {
      exchangeNames.emplace_back(kSupportedExchanges[exchangePos]);
    }
    ++exchangePos;
  }

  const auto marketDataPerExchange =
      _exchangesOrchestrator.getMarketDataPerExchange(marketPerPublicExchange, exchangeNames);

  // Transform data structures to export metrics input format
  MarketOrderBookConversionRates marketOrderBookConversionRates(marketDataPerExchange.size());
  TradesPerExchange lastTradesPerExchange(marketDataPerExchange.size());

  std::ranges::transform(marketDataPerExchange, marketOrderBookConversionRates.begin(),
                         [](const auto &exchangeWithPairOrderBooksAndTrades) {
                           return std::make_tuple(exchangeWithPairOrderBooksAndTrades.first->name(),
                                                  exchangeWithPairOrderBooksAndTrades.second.first, std::nullopt);
                         });

  std::ranges::transform(marketDataPerExchange, lastTradesPerExchange.begin(),
                         [](const auto &exchangeWithPairOrderBooksAndTrades) {
                           return std::make_pair(exchangeWithPairOrderBooksAndTrades.first,
                                                 exchangeWithPairOrderBooksAndTrades.second.second);
                         });

  _metricsExporter.exportOrderbookMetrics(marketOrderBookConversionRates);
  _metricsExporter.exportLastTradesMetrics(lastTradesPerExchange);
}

BalancePerExchange Coincenter::getBalance(std::span<const ExchangeName> privateExchangeNames,
                                          const BalanceOptions &balanceOptions) {
  CurrencyCode equiCurrency = balanceOptions.equiCurrency();
  const auto equiCur = _coincenterInfo.tryConvertStableCoinToFiat(equiCurrency);
  if (equiCur.isDefined()) {
    log::warn("Consider {} instead of stable coin {} as equivalent currency", equiCur, equiCurrency);
    equiCurrency = equiCur;
  }

  BalancePerExchange ret = _exchangesOrchestrator.getBalance(privateExchangeNames, balanceOptions);

  _metricsExporter.exportBalanceMetrics(ret, equiCurrency);

  return ret;
}

WalletPerExchange Coincenter::getDepositInfo(std::span<const ExchangeName> privateExchangeNames,
                                             CurrencyCode depositCurrency) {
  return _exchangesOrchestrator.getDepositInfo(privateExchangeNames, depositCurrency);
}

ClosedOrdersPerExchange Coincenter::getClosedOrders(std::span<const ExchangeName> privateExchangeNames,
                                                    const OrdersConstraints &closedOrdersConstraints) {
  return _exchangesOrchestrator.getClosedOrders(privateExchangeNames, closedOrdersConstraints);
}

OpenedOrdersPerExchange Coincenter::getOpenedOrders(std::span<const ExchangeName> privateExchangeNames,
                                                    const OrdersConstraints &openedOrdersConstraints) {
  return _exchangesOrchestrator.getOpenedOrders(privateExchangeNames, openedOrdersConstraints);
}

NbCancelledOrdersPerExchange Coincenter::cancelOrders(std::span<const ExchangeName> privateExchangeNames,
                                                      const OrdersConstraints &ordersConstraints) {
  return _exchangesOrchestrator.cancelOrders(privateExchangeNames, ordersConstraints);
}

DepositsPerExchange Coincenter::getRecentDeposits(std::span<const ExchangeName> privateExchangeNames,
                                                  const DepositsConstraints &depositsConstraints) {
  return _exchangesOrchestrator.getRecentDeposits(privateExchangeNames, depositsConstraints);
}

WithdrawsPerExchange Coincenter::getRecentWithdraws(std::span<const ExchangeName> privateExchangeNames,
                                                    const WithdrawsConstraints &withdrawsConstraints) {
  return _exchangesOrchestrator.getRecentWithdraws(privateExchangeNames, withdrawsConstraints);
}

TradedAmountsVectorWithFinalAmountPerExchange Coincenter::dustSweeper(
    std::span<const ExchangeName> privateExchangeNames, CurrencyCode currencyCode) {
  return _exchangesOrchestrator.dustSweeper(privateExchangeNames, currencyCode);
}

MonetaryAmountPerExchange Coincenter::getConversion(MonetaryAmount amount, CurrencyCode targetCurrencyCode,
                                                    ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getConversion(amount, targetCurrencyCode, exchangeNames);
}

ConversionPathPerExchange Coincenter::getConversionPaths(Market mk, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getConversionPaths(mk, exchangeNames);
}

CurrenciesPerExchange Coincenter::getCurrenciesPerExchange(ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getCurrenciesPerExchange(exchangeNames);
}

MarketsPerExchange Coincenter::getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2,
                                                     ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getMarketsPerExchange(cur1, cur2, exchangeNames);
}

UniquePublicSelectedExchanges Coincenter::getExchangesTradingCurrency(CurrencyCode currencyCode,
                                                                      ExchangeNameSpan exchangeNames,
                                                                      bool shouldBeWithdrawable) {
  return _exchangesOrchestrator.getExchangesTradingCurrency(currencyCode, exchangeNames, shouldBeWithdrawable);
}

UniquePublicSelectedExchanges Coincenter::getExchangesTradingMarket(Market mk, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getExchangesTradingMarket(mk, exchangeNames);
}

TradeResultPerExchange Coincenter::trade(MonetaryAmount startAmount, bool isPercentageTrade, CurrencyCode toCurrency,
                                         std::span<const ExchangeName> privateExchangeNames,
                                         const TradeOptions &tradeOptions) {
  return _exchangesOrchestrator.trade(startAmount, isPercentageTrade, toCurrency, privateExchangeNames, tradeOptions);
}

TradeResultPerExchange Coincenter::smartBuy(MonetaryAmount endAmount,
                                            std::span<const ExchangeName> privateExchangeNames,
                                            const TradeOptions &tradeOptions) {
  return _exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions);
}

TradeResultPerExchange Coincenter::smartSell(MonetaryAmount startAmount, bool isPercentageTrade,
                                             std::span<const ExchangeName> privateExchangeNames,
                                             const TradeOptions &tradeOptions) {
  return _exchangesOrchestrator.smartSell(startAmount, isPercentageTrade, privateExchangeNames, tradeOptions);
}

DeliveredWithdrawInfoWithExchanges Coincenter::withdraw(MonetaryAmount grossAmount, bool isPercentageWithdraw,
                                                        const ExchangeName &fromPrivateExchangeName,
                                                        const ExchangeName &toPrivateExchangeName,
                                                        const WithdrawOptions &withdrawOptions) {
  return _exchangesOrchestrator.withdraw(grossAmount, isPercentageWithdraw, fromPrivateExchangeName,
                                         toPrivateExchangeName, withdrawOptions);
}

MonetaryAmountByCurrencySetPerExchange Coincenter::getWithdrawFees(CurrencyCode currencyCode,
                                                                   ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getWithdrawFees(currencyCode, exchangeNames);
}

MonetaryAmountPerExchange Coincenter::getLast24hTradedVolumePerExchange(Market mk, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getLast24hTradedVolumePerExchange(mk, exchangeNames);
}

TradesPerExchange Coincenter::getLastTradesPerExchange(Market mk, ExchangeNameSpan exchangeNames,
                                                       std::optional<int> depth) {
  const auto ret = _exchangesOrchestrator.getLastTradesPerExchange(mk, exchangeNames, depth);

  _metricsExporter.exportLastTradesMetrics(ret);

  return ret;
}

MonetaryAmountPerExchange Coincenter::getLastPricePerExchange(Market mk, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getLastPricePerExchange(mk, exchangeNames);
}

MarketsPerExchange Coincenter::getMarketsAvailableForReplay(const ReplayOptions &replayOptions,
                                                            ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.pullAvailableMarketsForReplay(replayOptions.timeWindow(), exchangeNames);
}

namespace {
auto CreateExchangeNameVector(Market market, const MarketsPerExchange &marketsPerExchange) {
  PublicExchangeNameVector exchangesWithThisMarketData;
  for (const auto &[exchange, markets] : marketsPerExchange) {
    if (markets.contains(market)) {
      exchangesWithThisMarketData.emplace_back(exchange->name());
    }
  }
  return exchangesWithThisMarketData;
}

auto CreateMarkets(const MarketsPerExchange &marketsPerExchange, Market market) {
  MarketSet allMarkets;

  if (market.isDefined()) {
    allMarkets.insert(std::move(market));
  } else {
    for (const auto &[exchange, markets] : marketsPerExchange) {
      std::ranges::copy(markets, std::inserter(allMarkets, allMarkets.end()));
    }
  }

  return allMarkets;
}

void CreateAndRegisterTraderAlgorithms(const MarketTraderFactory &marketTraderFactory,
                                       std::span<MarketTraderEngine> marketTraderEngines) {
  for (auto &marketTraderEngine : marketTraderEngines) {
    const auto &marketTraderEngineState = marketTraderEngine.marketTraderEngineState();

    marketTraderEngine.registerMarketTrader(marketTraderFactory.construct(marketTraderEngineState));
  }
}

}  // namespace

void Coincenter::replay(const ReplayOptions &replayOptions, Market market, ExchangeNameSpan exchangeNames) {
  const TimeWindow timeWindow = replayOptions.timeWindow();
  const auto marketsPerExchange = _exchangesOrchestrator.pullAvailableMarketsForReplay(timeWindow, exchangeNames);

  ReplayAlgorithmNameIterator replayAlgorithmNameIterator(replayOptions.algorithmNames(),
                                                          MarketTraderFactory::kAllAlgorithms);

  while (replayAlgorithmNameIterator.hasNext()) {
    std::string_view algorithmName = replayAlgorithmNameIterator.next();

    MarketTraderFactory marketTraderFactory(algorithmName);

    for (const Market market : CreateMarkets(marketsPerExchange, market)) {
      auto exchangesWithThisMarketData = CreateExchangeNameVector(market, marketsPerExchange);
      if (exchangesWithThisMarketData.empty()) {
        continue;
      }

      // Create the MarketTraderEngines based on this market, filtering out exchanges without available amount to trade
      MarketTraderEngineVector marketTraderEngines =
          createMarketTraderEngines(replayOptions, market, exchangesWithThisMarketData);

      replayAlgorithm(marketTraderFactory, replayOptions, marketTraderEngines, exchangesWithThisMarketData);
    }
  }
}

void Coincenter::replayAlgorithm(const MarketTraderFactory &marketTraderFactory, const ReplayOptions &replayOptions,
                                 std::span<MarketTraderEngine> marketTraderEngines,
                                 const PublicExchangeNameVector &exchangesWithThisMarketData) {
  CreateAndRegisterTraderAlgorithms(marketTraderFactory, marketTraderEngines);

  MarketTradeRangeStatsPerExchange tradeRangeStatsPerExchange =
      tradingProcess(replayOptions, marketTraderEngines, exchangesWithThisMarketData);

  // Finally retrieve and print results for this market
  MarketTradingGlobalResultPerExchange marketTradingResultPerExchange =
      _exchangesOrchestrator.getMarketTraderResultPerExchange(
          marketTraderEngines, std::move(tradeRangeStatsPerExchange), exchangesWithThisMarketData);

  _queryResultPrinter.printMarketTradingResults(replayOptions.timeWindow(), marketTradingResultPerExchange,
                                                CoincenterCommandType::kReplay);
}

Coincenter::MarketTraderEngineVector Coincenter::createMarketTraderEngines(
    const ReplayOptions &replayOptions, Market market, PublicExchangeNameVector &exchangesWithThisMarketData) {
  auto nbExchanges = exchangesWithThisMarketData.size();

  const auto &automationConfig = _coincenterInfo.generalConfig().tradingConfig().automationConfig();
  const auto startBaseAmountEquivalent = automationConfig.startBaseAmountEquivalent();
  const auto startQuoteAmountEquivalent = automationConfig.startQuoteAmountEquivalent();
  const bool isValidateOnly = replayOptions.replayMode() == ReplayOptions::ReplayMode::kValidateOnly;

  auto convertedBaseAmountPerExchange =
      isValidateOnly ? MonetaryAmountPerExchange{}
                     : getConversion(startBaseAmountEquivalent, market.base(), exchangesWithThisMarketData);
  auto convertedQuoteAmountPerExchange =
      isValidateOnly ? MonetaryAmountPerExchange{}
                     : getConversion(startQuoteAmountEquivalent, market.quote(), exchangesWithThisMarketData);

  MarketTraderEngineVector marketTraderEngines;
  for (decltype(nbExchanges) exchangePos = 0; exchangePos < nbExchanges; ++exchangePos) {
    MonetaryAmount startBaseAmount =
        isValidateOnly ? MonetaryAmount{0, market.base()} : convertedBaseAmountPerExchange[exchangePos].second;
    MonetaryAmount startQuoteAmount =
        isValidateOnly ? MonetaryAmount{0, market.quote()} : convertedQuoteAmountPerExchange[exchangePos].second;

    if (startBaseAmount.currencyCode() != market.base()) {
      // This is possible as conversion may use equivalent fiats and stable coins
      log::info("Target converted currency is different from market one, replace with market currency {} -> {}",
                startBaseAmount.currencyCode(), market.base());
      startBaseAmount = MonetaryAmount(startBaseAmount.amount(), market.base(), startBaseAmount.nbDecimals());
    }
    if (startQuoteAmount.currencyCode() != market.quote()) {
      // This is possible as conversion may use equivalent fiats and stable coins
      log::info("Target converted currency is different from market one, replace with market currency {} -> {}",
                startQuoteAmount.currencyCode(), market.quote());
      startQuoteAmount = MonetaryAmount(startQuoteAmount.amount(), market.quote(), startQuoteAmount.nbDecimals());
    }

    if (!isValidateOnly && (startBaseAmount == 0 || startQuoteAmount == 0)) {
      log::warn("Cannot convert to start base / quote amounts for {} ({} / {})",
                exchangesWithThisMarketData[exchangePos], startBaseAmount, startQuoteAmount);
      exchangesWithThisMarketData.erase(exchangesWithThisMarketData.begin() + exchangePos);
      convertedBaseAmountPerExchange.erase(convertedBaseAmountPerExchange.begin() + exchangePos);
      convertedQuoteAmountPerExchange.erase(convertedQuoteAmountPerExchange.begin() + exchangePos);
      --exchangePos;
      --nbExchanges;
      continue;
    }

    const ExchangeConfig &exchangeConfig =
        _coincenterInfo.exchangeConfig(exchangesWithThisMarketData[exchangePos].name());

    marketTraderEngines.emplace_back(exchangeConfig, market, startBaseAmount, startQuoteAmount);
  }
  return marketTraderEngines;
}

MarketTradeRangeStatsPerExchange Coincenter::tradingProcess(const ReplayOptions &replayOptions,
                                                            std::span<MarketTraderEngine> marketTraderEngines,
                                                            ExchangeNameSpan exchangesWithThisMarketData) {
  const auto &automationConfig = _coincenterInfo.generalConfig().tradingConfig().automationConfig();
  const auto loadChunkDuration = automationConfig.loadChunkDuration();
  const auto timeWindow = replayOptions.timeWindow();

  MarketTradeRangeStatsPerExchange tradeRangeResultsPerExchange;

  // Main loop - parallelized by exchange, with time window chunks of loadChunkDuration

  TimeWindow subTimeWindow(timeWindow.from(), loadChunkDuration);
  while (subTimeWindow.overlaps(timeWindow)) {
    auto subRangeResultsPerExchange = _exchangesOrchestrator.traderConsumeRange(
        replayOptions, subTimeWindow, marketTraderEngines, exchangesWithThisMarketData);

    if (tradeRangeResultsPerExchange.empty()) {
      tradeRangeResultsPerExchange = std::move(subRangeResultsPerExchange);
    } else {
      int pos{};
      for (auto &[exchange, result] : subRangeResultsPerExchange) {
        tradeRangeResultsPerExchange[pos].second += result;
        ++pos;
      }
    }

    // Go to next sub time window
    subTimeWindow = TimeWindow(subTimeWindow.to(), loadChunkDuration);
  }

  return tradeRangeResultsPerExchange;
}

void Coincenter::updateFileCaches() const {
  log::debug("Store all cache files");

  _commonAPI.updateCacheFile();
  _fiatConverter.updateCacheFile();

  std::ranges::for_each(_exchangePool.exchanges(), [](const Exchange &exchange) { exchange.updateCacheFile(); });
}

}  // namespace cct
