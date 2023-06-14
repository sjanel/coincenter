#include "coincenter.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include "abstractmetricgateway.hpp"
#include "coincentercommands.hpp"
#include "coincentercommandtype.hpp"
#include "coincenteroptions.hpp"
#include "queryresultprinter.hpp"
#include "stringoptionparser.hpp"

namespace cct {
using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

Coincenter::Coincenter(const CoincenterInfo &coincenterInfo, const ExchangeSecretsInfo &exchangeSecretsInfo)
    : _coincenterInfo(coincenterInfo),
      _cryptowatchAPI(coincenterInfo, coincenterInfo.getRunMode()),
      _fiatConverter(coincenterInfo, coincenterInfo.fiatConversionQueryRate()),
      _apiKeyProvider(coincenterInfo.dataDir(), exchangeSecretsInfo, coincenterInfo.getRunMode()),
      _metricsExporter(coincenterInfo.metricGatewayPtr()),
      _exchangePool(coincenterInfo, _fiatConverter, _cryptowatchAPI, _apiKeyProvider),
      _exchangesOrchestrator(_exchangePool.exchanges()),
      _queryResultPrinter(coincenterInfo.apiOutputType()) {}

int Coincenter::process(const CoincenterCommands &coincenterCommands) {
  int nbCommandsProcessed = 0;
  auto commands = coincenterCommands.commands();
  const int nbRepeats = commands.empty() ? 0 : coincenterCommands.repeats();
  for (int repeatPos = 0; repeatPos != nbRepeats; ++repeatPos) {
    if (repeatPos != 0) {
      std::this_thread::sleep_for(coincenterCommands.repeatTime());
    }
    if (nbRepeats != 1) {
      if (nbRepeats == -1) {
        log::info("Processing request {}", repeatPos + 1);
      } else {
        log::info("Processing request {}/{}", repeatPos + 1, nbRepeats);
      }
    }
    for (const CoincenterCommand &cmd : commands) {
      processCommand(cmd);
      ++nbCommandsProcessed;
    }
  }
  return nbCommandsProcessed;
}

void Coincenter::processCommand(const CoincenterCommand &cmd) {
  switch (cmd.type()) {
    case CoincenterCommandType::kHealthCheck: {
      ExchangeHealthCheckStatus healthCheckStatus = healthCheck(cmd.exchangeNames());
      _queryResultPrinter.printHealthCheck(healthCheckStatus);
      break;
    }
    case CoincenterCommandType::kMarkets: {
      MarketsPerExchange marketsPerExchange = getMarketsPerExchange(cmd.cur1(), cmd.cur2(), cmd.exchangeNames());
      _queryResultPrinter.printMarkets(cmd.cur1(), cmd.cur2(), marketsPerExchange);
      break;
    }
    case CoincenterCommandType::kConversionPath: {
      ConversionPathPerExchange conversionPathPerExchange = getConversionPaths(cmd.market(), cmd.exchangeNames());
      _queryResultPrinter.printConversionPath(cmd.market(), conversionPathPerExchange);
      break;
    }
    case CoincenterCommandType::kLastPrice: {
      MonetaryAmountPerExchange lastPricePerExchange = getLastPricePerExchange(cmd.market(), cmd.exchangeNames());
      _queryResultPrinter.printLastPrice(cmd.market(), lastPricePerExchange);
      break;
    }
    case CoincenterCommandType::kTicker: {
      ExchangeTickerMaps exchangeTickerMaps = getTickerInformation(cmd.exchangeNames());
      _queryResultPrinter.printTickerInformation(exchangeTickerMaps);
      break;
    }
    case CoincenterCommandType::kOrderbook: {
      MarketOrderBookConversionRates marketOrderBooksConversionRates =
          getMarketOrderBooks(cmd.market(), cmd.exchangeNames(), cmd.cur1(), cmd.optDepth());
      _queryResultPrinter.printMarketOrderBooks(cmd.market(), cmd.cur1(), cmd.optDepth(),
                                                marketOrderBooksConversionRates);
      break;
    }
    case CoincenterCommandType::kLastTrades: {
      LastTradesPerExchange lastTradesPerExchange =
          getLastTradesPerExchange(cmd.market(), cmd.exchangeNames(), cmd.nbLastTrades());
      _queryResultPrinter.printLastTrades(cmd.market(), cmd.nbLastTrades(), lastTradesPerExchange);
      break;
    }
    case CoincenterCommandType::kLast24hTradedVolume: {
      MonetaryAmountPerExchange tradedVolumePerExchange =
          getLast24hTradedVolumePerExchange(cmd.market(), cmd.exchangeNames());
      _queryResultPrinter.printLast24hTradedVolume(cmd.market(), tradedVolumePerExchange);
      break;
    }
    case CoincenterCommandType::kWithdrawFee: {
      auto withdrawFeesPerExchange = getWithdrawFees(cmd.cur1(), cmd.exchangeNames());
      _queryResultPrinter.printWithdrawFees(withdrawFeesPerExchange, cmd.cur1());
      break;
    }

    case CoincenterCommandType::kBalance: {
      BalanceOptions balanceOptions(cmd.withBalanceInUse() ? BalanceOptions::AmountIncludePolicy::kWithBalanceInUse
                                                           : BalanceOptions::AmountIncludePolicy::kOnlyAvailable,
                                    cmd.cur1());
      BalancePerExchange balancePerExchange = getBalance(cmd.exchangeNames(), balanceOptions);
      _queryResultPrinter.printBalance(balancePerExchange, cmd.cur1());
      break;
    }
    case CoincenterCommandType::kDepositInfo: {
      WalletPerExchange walletPerExchange = getDepositInfo(cmd.exchangeNames(), cmd.cur1());
      _queryResultPrinter.printDepositInfo(cmd.cur1(), walletPerExchange);
      break;
    }
    case CoincenterCommandType::kOrdersOpened: {
      OpenedOrdersPerExchange openedOrdersPerExchange = getOpenedOrders(cmd.exchangeNames(), cmd.ordersConstraints());
      _queryResultPrinter.printOpenedOrders(openedOrdersPerExchange, cmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::kOrdersCancel: {
      NbCancelledOrdersPerExchange nbCancelledOrdersPerExchange =
          cancelOrders(cmd.exchangeNames(), cmd.ordersConstraints());
      _queryResultPrinter.printCancelledOrders(nbCancelledOrdersPerExchange, cmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::kRecentDeposits: {
      DepositsPerExchange depositsPerExchange =
          getRecentDeposits(cmd.exchangeNames(), cmd.withdrawsOrDepositsConstraints());
      _queryResultPrinter.printRecentDeposits(depositsPerExchange, cmd.withdrawsOrDepositsConstraints());
      break;
    }
    case CoincenterCommandType::kRecentWithdraws: {
      WithdrawsPerExchange withdrawsPerExchange =
          getRecentWithdraws(cmd.exchangeNames(), cmd.withdrawsOrDepositsConstraints());
      _queryResultPrinter.printRecentWithdraws(withdrawsPerExchange, cmd.withdrawsOrDepositsConstraints());
      break;
    }
    case CoincenterCommandType::kTrade: {
      TradedAmountsPerExchange tradedAmountsPerExchange =
          trade(cmd.amount(), cmd.isPercentageAmount(), cmd.cur1(), cmd.exchangeNames(), cmd.tradeOptions());
      _queryResultPrinter.printTrades(tradedAmountsPerExchange, cmd.amount(), cmd.isPercentageAmount(), cmd.cur1(),
                                      cmd.tradeOptions());
      break;
    }
    case CoincenterCommandType::kBuy: {
      TradedAmountsPerExchange tradedAmountsPerExchange =
          smartBuy(cmd.amount(), cmd.exchangeNames(), cmd.tradeOptions());
      _queryResultPrinter.printBuyTrades(tradedAmountsPerExchange, cmd.amount(), cmd.tradeOptions());
      break;
    }
    case CoincenterCommandType::kSell: {
      TradedAmountsPerExchange tradedAmountsPerExchange =
          smartSell(cmd.amount(), cmd.isPercentageAmount(), cmd.exchangeNames(), cmd.tradeOptions());
      _queryResultPrinter.printSellTrades(tradedAmountsPerExchange, cmd.amount(), cmd.isPercentageAmount(),
                                          cmd.tradeOptions());
      break;
    }
    case CoincenterCommandType::kWithdraw: {
      const ExchangeName &fromExchangeName = cmd.exchangeNames().front();
      const ExchangeName &toExchangeName = cmd.exchangeNames().back();
      DeliveredWithdrawInfoWithExchanges deliveredWithdrawInfoWithExchanges =
          withdraw(cmd.amount(), cmd.isPercentageAmount(), fromExchangeName, toExchangeName, cmd.withdrawOptions());
      _queryResultPrinter.printWithdraw(deliveredWithdrawInfoWithExchanges, cmd.isPercentageAmount(),
                                        cmd.withdrawOptions());
      break;
    }
    case CoincenterCommandType::kDustSweeper: {
      _queryResultPrinter.printDustSweeper(dustSweeper(cmd.exchangeNames(), cmd.cur1()), cmd.cur1());
      break;
    }
    default:
      throw exception("Unknown command type");
  }
}

ExchangeHealthCheckStatus Coincenter::healthCheck(ExchangeNameSpan exchangeNames) {
  ExchangeHealthCheckStatus ret = _exchangesOrchestrator.healthCheck(exchangeNames);

  _metricsExporter.exportHealthCheckMetrics(ret);

  return ret;
}

ExchangeTickerMaps Coincenter::getTickerInformation(ExchangeNameSpan exchangeNames) {
  ExchangeTickerMaps ret = _exchangesOrchestrator.getTickerInformation(exchangeNames);

  _metricsExporter.exportTickerMetrics(ret);

  return ret;
}

MarketOrderBookConversionRates Coincenter::getMarketOrderBooks(Market mk, ExchangeNameSpan exchangeNames,
                                                               CurrencyCode equiCurrencyCode,
                                                               std::optional<int> depth) {
  MarketOrderBookConversionRates ret =
      _exchangesOrchestrator.getMarketOrderBooks(mk, exchangeNames, equiCurrencyCode, depth);

  _metricsExporter.exportOrderbookMetrics(mk, ret);

  return ret;
}

BalancePerExchange Coincenter::getBalance(std::span<const ExchangeName> privateExchangeNames,
                                          const BalanceOptions &balanceOptions) {
  CurrencyCode equiCurrency = balanceOptions.equiCurrency();
  std::optional<CurrencyCode> optEquiCur = _coincenterInfo.fiatCurrencyIfStableCoin(equiCurrency);
  if (optEquiCur) {
    log::warn("Consider {} instead of stable coin {} as equivalent currency", *optEquiCur, equiCurrency);
    equiCurrency = *optEquiCur;
  }

  BalancePerExchange ret = _exchangesOrchestrator.getBalance(privateExchangeNames, balanceOptions);

  _metricsExporter.exportBalanceMetrics(ret, equiCurrency);

  return ret;
}

WalletPerExchange Coincenter::getDepositInfo(std::span<const ExchangeName> privateExchangeNames,
                                             CurrencyCode depositCurrency) {
  return _exchangesOrchestrator.getDepositInfo(privateExchangeNames, depositCurrency);
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

ConversionPathPerExchange Coincenter::getConversionPaths(Market mk, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getConversionPaths(mk, exchangeNames);
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

TradedAmountsPerExchange Coincenter::trade(MonetaryAmount startAmount, bool isPercentageTrade, CurrencyCode toCurrency,
                                           std::span<const ExchangeName> privateExchangeNames,
                                           const TradeOptions &tradeOptions) {
  return _exchangesOrchestrator.trade(startAmount, isPercentageTrade, toCurrency, privateExchangeNames, tradeOptions);
}

TradedAmountsPerExchange Coincenter::smartBuy(MonetaryAmount endAmount,
                                              std::span<const ExchangeName> privateExchangeNames,
                                              const TradeOptions &tradeOptions) {
  return _exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions);
}

TradedAmountsPerExchange Coincenter::smartSell(MonetaryAmount startAmount, bool isPercentageTrade,
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

MonetaryAmountPerExchange Coincenter::getWithdrawFees(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getWithdrawFees(currencyCode, exchangeNames);
}

MonetaryAmountPerExchange Coincenter::getLast24hTradedVolumePerExchange(Market mk, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getLast24hTradedVolumePerExchange(mk, exchangeNames);
}

LastTradesPerExchange Coincenter::getLastTradesPerExchange(Market mk, ExchangeNameSpan exchangeNames,
                                                           int nbLastTrades) {
  LastTradesPerExchange ret = _exchangesOrchestrator.getLastTradesPerExchange(mk, exchangeNames, nbLastTrades);

  _metricsExporter.exportLastTradesMetrics(mk, ret);

  return ret;
}

MonetaryAmountPerExchange Coincenter::getLastPricePerExchange(Market mk, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getLastPricePerExchange(mk, exchangeNames);
}

void Coincenter::updateFileCaches() const {
  log::debug("Store all cache files");
  _cryptowatchAPI.updateCacheFile();
  _fiatConverter.updateCacheFile();
  auto exchanges = _exchangePool.exchanges();
  std::for_each(exchanges.begin(), exchanges.end(), [](const Exchange &exchange) { exchange.updateCacheFile(); });
}

}  // namespace cct
