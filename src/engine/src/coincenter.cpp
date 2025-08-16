#include "coincenter.hpp"

#include <algorithm>
#include <array>
#include <csignal>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "abstract-market-trader-factory.hpp"
#include "algorithm-name-iterator.hpp"
#include "auto-trade-processor.hpp"
#include "balanceoptions.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
#include "enum-string.hpp"
#include "exchange-name-enum.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "exchangeretriever.hpp"
#include "exchangesecretsinfo.hpp"
#include "market-timestamp-set.hpp"
#include "market-trader-engine.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "query-result-type-helpers.hpp"
#include "queryresulttypes.hpp"
#include "replay-options.hpp"
#include "signal-handler.hpp"
#include "time-window.hpp"
#include "timedef.hpp"
#include "withdrawsconstraints.hpp"

namespace cct {

using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

Coincenter::Coincenter(const CoincenterInfo &coincenterInfo, const ExchangeSecretsInfo &exchangeSecretsInfo)
    : _coincenterInfo(coincenterInfo),
      _commonAPI(coincenterInfo),
      _fiatConverter(coincenterInfo, coincenterInfo.fiatConversionQueryRate()),
      _apiKeyProvider(coincenterInfo.dataDir(), exchangeSecretsInfo, coincenterInfo.getRunMode()),
      _metricsExporter(coincenterInfo.metricGatewayPtr()),
      _exchangePool(coincenterInfo, _fiatConverter, _commonAPI, _apiKeyProvider),
      _exchangesOrchestrator(coincenterInfo.generalConfig().requests, _exchangePool.exchanges()) {}

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

MarketDataPerExchange Coincenter::queryMarketDataPerExchange(std::span<const Market> marketPerPublicExchangePos) {
  ExchangeNameEnumVector exchangeNameEnums;

  int exchangePos{};
  for (Market market : marketPerPublicExchangePos) {
    if (market.isDefined()) {
      exchangeNameEnums.emplace_back(static_cast<ExchangeNameEnum>(exchangePos));
    }
    ++exchangePos;
  }

  const auto marketDataPerExchange =
      _exchangesOrchestrator.getMarketDataPerExchange(marketPerPublicExchangePos, exchangeNameEnums);

  // Transform data structures to export metrics input format
  MarketOrderBookConversionRates marketOrderBookConversionRates(marketDataPerExchange.size());
  TradesPerExchange lastTradesPerExchange(marketDataPerExchange.size());

  std::ranges::transform(marketDataPerExchange, marketOrderBookConversionRates.begin(),
                         [](const auto &exchangeWithPairOrderBooksAndTrades) {
                           return std::make_tuple(exchangeWithPairOrderBooksAndTrades.first->exchangeNameEnum(),
                                                  exchangeWithPairOrderBooksAndTrades.second.first, std::nullopt);
                         });

  std::ranges::transform(marketDataPerExchange, lastTradesPerExchange.begin(),
                         [](const auto &exchangeWithPairOrderBooksAndTrades) {
                           return std::make_pair(exchangeWithPairOrderBooksAndTrades.first,
                                                 exchangeWithPairOrderBooksAndTrades.second.second);
                         });

  _metricsExporter.exportOrderbookMetrics(marketOrderBookConversionRates);
  _metricsExporter.exportLastTradesMetrics(lastTradesPerExchange);

  return marketDataPerExchange;
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
                                                    ExchangeNameEnumSpan exchangeNameEnums) {
  return _exchangesOrchestrator.getConversion(amount, targetCurrencyCode, exchangeNameEnums);
}

MonetaryAmountPerExchange Coincenter::getConversion(std::span<const MonetaryAmount> monetaryAmountPerExchangeToConvert,
                                                    CurrencyCode targetCurrencyCode,
                                                    ExchangeNameEnumSpan exchangeNameEnums) {
  return _exchangesOrchestrator.getConversion(monetaryAmountPerExchangeToConvert, targetCurrencyCode,
                                              exchangeNameEnums);
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

MarketTimestampSetsPerExchange Coincenter::getMarketsAvailableForReplay(const ReplayOptions &replayOptions,
                                                                        ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.pullAvailableMarketsForReplay(replayOptions.timeWindow(), exchangeNames);
}

namespace {
auto CreateExchangeNameVector(Market market, const MarketTimestampSetsPerExchange &marketTimestampSetsPerExchange) {
  ExchangeNameEnumVector exchangesWithThisMarketData;
  for (const auto &[exchange, marketTimestampSets] : marketTimestampSetsPerExchange) {
    if (ContainsMarket(market, marketTimestampSets)) {
      exchangesWithThisMarketData.emplace_back(exchange->exchangeNameEnum());
    }
  }
  return exchangesWithThisMarketData;
}

void CreateAndRegisterTraderAlgorithms(const AbstractMarketTraderFactory &marketTraderFactory,
                                       std::string_view algorithmName,
                                       std::span<MarketTraderEngine> marketTraderEngines) {
  for (auto &marketTraderEngine : marketTraderEngines) {
    const auto &marketTraderEngineState = marketTraderEngine.marketTraderEngineState();

    marketTraderEngine.registerMarketTrader(marketTraderFactory.construct(algorithmName, marketTraderEngineState));
  }
}

bool Filter(Market market, MarketTimestampSet &marketTimestampSet) {
  auto it = std::ranges::partition_point(
      marketTimestampSet, [market](const auto &marketTimestamp) { return marketTimestamp.market < market; });
  if (it != marketTimestampSet.end() && it->market == market) {
    marketTimestampSet = MarketTimestampSet{*it};
    return false;
  }

  marketTimestampSet.clear();
  return true;
}

void Filter(Market market, MarketTimestampSetsPerExchange &marketTimestampSetsPerExchange) {
  for (auto it = marketTimestampSetsPerExchange.begin(); it != marketTimestampSetsPerExchange.end();) {
    const bool orderBooksEmpty = Filter(market, it->second.orderBooksMarkets);
    const bool tradesEmpty = Filter(market, it->second.tradesMarkets);

    if (orderBooksEmpty && tradesEmpty) {
      // no more data, remove the exchange entry completely
      it = marketTimestampSetsPerExchange.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace

ReplayResults Coincenter::replay(const AbstractMarketTraderFactory &marketTraderFactory,
                                 const ReplayOptions &replayOptions, Market market, ExchangeNameSpan exchangeNames) {
  const TimeWindow timeWindow = replayOptions.timeWindow();
  auto marketTimestampSetsPerExchange = _exchangesOrchestrator.pullAvailableMarketsForReplay(timeWindow, exchangeNames);

  if (market.isDefined()) {
    Filter(market, marketTimestampSetsPerExchange);
  }

  const MarketSet allMarkets = ComputeAllMarkets(marketTimestampSetsPerExchange);

  ReplayResults replayResults;

  AlgorithmNameIterator replayAlgorithmNameIterator(replayOptions.algorithmNames(),
                                                    marketTraderFactory.allSupportedAlgorithms());

  while (replayAlgorithmNameIterator.hasNext()) {
    std::string_view algorithmName = replayAlgorithmNameIterator.next();

    ReplayResults::mapped_type algorithmResults;

    algorithmResults.reserve(allMarkets.size());

    for (const Market replayMarket : allMarkets) {
      auto exchangesWithThisMarketData = CreateExchangeNameVector(replayMarket, marketTimestampSetsPerExchange);

      // Create the MarketTraderEngines based on this market, filtering out exchanges without available amount to
      // trade
      auto marketTraderEngines =
          createMarketTraderEnginesForReplay(replayOptions, replayMarket, exchangesWithThisMarketData);

      MarketTradingGlobalResultPerExchange marketTradingResultPerExchange = replayAlgorithm(
          marketTraderFactory, algorithmName, replayOptions, marketTraderEngines, exchangesWithThisMarketData);

      algorithmResults.push_back(std::move(marketTradingResultPerExchange));
    }

    replayResults.insert({algorithmName, std::move(algorithmResults)});
  }

  return replayResults;
}

void Coincenter::autoTrade(const AutoTradeOptions &autoTradeOptions) {
  AutoTradeProcessor autoTradeProcessor(autoTradeOptions);

  auto marketTraderEngines = autoTradeProcessor.createMarketTraderEngines(_coincenterInfo);

  while (!IsStopRequested()) {
    // 1: select exchanges positions for which we are allowed to send a request.
    AutoTradeProcessor::SelectedMarketVector selectedMarkets = autoTradeProcessor.computeSelectedMarkets();
    if (selectedMarkets.empty()) {
      break;
    }

    // 2: Query order books for those exchanges
    std::array<Market, kNbSupportedExchanges> selectedMarketsPerPublicExchangePos;
    for (const AutoTradeProcessor::SelectedMarket &selectedMarket : selectedMarkets) {
      const auto publicExchangePos = selectedMarket.privateExchangeNames.front().publicExchangePos();
      selectedMarketsPerPublicExchangePos[publicExchangePos] = selectedMarket.market;
    }
    MarketDataPerExchange marketDataPerExchange = queryMarketDataPerExchange(selectedMarketsPerPublicExchangePos);

    // 3: call algorithms and retrieve their actions
    // 4: perform actions (Trades, cancel, exit criteria)
  }
}

MarketTradingGlobalResultPerExchange Coincenter::replayAlgorithm(
    const AbstractMarketTraderFactory &marketTraderFactory, std::string_view algorithmName,
    const ReplayOptions &replayOptions, std::span<MarketTraderEngine> marketTraderEngines,
    const ExchangeNameEnumVector &exchangesWithThisMarketData) {
  CreateAndRegisterTraderAlgorithms(marketTraderFactory, algorithmName, marketTraderEngines);

  MarketTradeRangeStatsPerExchange tradeRangeStatsPerExchange =
      tradingProcess(replayOptions, marketTraderEngines, exchangesWithThisMarketData);

  return _exchangesOrchestrator.getMarketTraderResultPerExchange(
      marketTraderEngines, std::move(tradeRangeStatsPerExchange), exchangesWithThisMarketData);
}

namespace {
MonetaryAmount ComputeStartAmount(CurrencyCode currencyCode, MonetaryAmount convertedAmount) {
  if (convertedAmount.currencyCode() != currencyCode) {
    // This is possible as conversion may use equivalent fiats and stable coins
    log::info("Target converted currency is different from market one, replace with market currency {} -> {}",
              convertedAmount.currencyCode(), currencyCode);
    return {convertedAmount.amount(), currencyCode, convertedAmount.nbDecimals()};
  }

  return convertedAmount;
}
}  // namespace

Coincenter::MarketTraderEngineVector Coincenter::createMarketTraderEnginesForReplay(
    const ReplayOptions &replayOptions, Market market, ExchangeNameEnumVector &exchangesWithThisMarketData) {
  const auto &automationConfig = _coincenterInfo.generalConfig().trading.automation;
  const auto startBaseAmountEquivalent = automationConfig.startingContext.startBaseAmountEquivalent;
  const auto startQuoteAmountEquivalent = automationConfig.startingContext.startQuoteAmountEquivalent;
  const bool isValidateOnly = replayOptions.replayMode() == ReplayOptions::ReplayMode::kValidateOnly;

  auto convertedBaseAmountPerExchange =
      isValidateOnly ? MonetaryAmountPerExchange{}
                     : getConversion(startBaseAmountEquivalent, market.base(), exchangesWithThisMarketData);
  auto convertedQuoteAmountPerExchange =
      isValidateOnly ? MonetaryAmountPerExchange{}
                     : getConversion(startQuoteAmountEquivalent, market.quote(), exchangesWithThisMarketData);

  MarketTraderEngineVector marketTraderEngines;
  for (ExchangeNameEnumVector::size_type exchangePos{}; exchangePos < exchangesWithThisMarketData.size();
       ++exchangePos) {
    const auto startBaseAmount =
        isValidateOnly ? MonetaryAmount{0, market.base()}
                       : ComputeStartAmount(market.base(), convertedBaseAmountPerExchange[exchangePos].second);
    const auto startQuoteAmount =
        isValidateOnly ? MonetaryAmount{0, market.quote()}
                       : ComputeStartAmount(market.quote(), convertedQuoteAmountPerExchange[exchangePos].second);

    if (!isValidateOnly && (startBaseAmount == 0 || startQuoteAmount == 0)) {
      log::warn("Cannot convert to start base / quote amounts for {} ({} / {})",
                EnumToString(exchangesWithThisMarketData[exchangePos]), startBaseAmount, startQuoteAmount);
      exchangesWithThisMarketData.erase(exchangesWithThisMarketData.begin() + exchangePos);
      convertedBaseAmountPerExchange.erase(convertedBaseAmountPerExchange.begin() + exchangePos);
      convertedQuoteAmountPerExchange.erase(convertedQuoteAmountPerExchange.begin() + exchangePos);
      --exchangePos;
      continue;
    }

    const auto &exchangeConfig = _coincenterInfo.exchangeConfig(exchangesWithThisMarketData[exchangePos]);

    marketTraderEngines.emplace_back(exchangeConfig, market, startBaseAmount, startQuoteAmount);
  }
  return marketTraderEngines;
}

MarketTradeRangeStatsPerExchange Coincenter::tradingProcess(const ReplayOptions &replayOptions,
                                                            std::span<MarketTraderEngine> marketTraderEngines,
                                                            ExchangeNameEnumSpan exchangesWithThisMarketData) {
  const auto &automationConfig = _coincenterInfo.generalConfig().trading.automation;
  const auto loadChunkDuration = automationConfig.deserialization.loadChunkDuration.duration;
  const auto timeWindow = replayOptions.timeWindow();

  MarketTradeRangeStatsPerExchange tradeRangeResultsPerExchange;

  // Main loop - parallelized by exchange, with time window chunks of loadChunkDuration

  for (TimeWindow subTimeWindow(timeWindow.from(), loadChunkDuration); subTimeWindow.overlaps(timeWindow);
       subTimeWindow += loadChunkDuration) {
    auto subRangeResultsPerExchange = _exchangesOrchestrator.traderConsumeRange(
        replayOptions, subTimeWindow, marketTraderEngines, exchangesWithThisMarketData);

    if (tradeRangeResultsPerExchange.empty()) {
      tradeRangeResultsPerExchange = std::move(subRangeResultsPerExchange);
    } else {
      int pos{};  // TODO: we can use std::views::enumerate from C++23 when available
      for (auto &[exchange, result] : subRangeResultsPerExchange) {
        tradeRangeResultsPerExchange[pos].second += result;
        ++pos;
      }
    }
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
