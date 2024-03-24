#include "coincenter.hpp"

#include <algorithm>
#include <csignal>
#include <optional>
#include <span>
#include <thread>
#include <utility>

#include "balanceoptions.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
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
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "queryresultprinter.hpp"
#include "queryresulttypes.hpp"
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
    for (const auto &cmd : commands) {
      transferableResults = processCommand(cmd, transferableResults);
      ++nbCommandsProcessed;
    }
  }
  return nbCommandsProcessed;
}

TransferableCommandResultVector Coincenter::processCommand(
    const CoincenterCommand &cmd, std::span<const TransferableCommandResult> previousTransferableResults) {
  TransferableCommandResultVector transferableResults;
  switch (cmd.type()) {
    case CoincenterCommandType::kHealthCheck: {
      const auto healthCheckStatus = healthCheck(cmd.exchangeNames());
      _queryResultPrinter.printHealthCheck(healthCheckStatus);
      break;
    }
    case CoincenterCommandType::kCurrencies: {
      const auto currenciesPerExchange = getCurrenciesPerExchange(cmd.exchangeNames());
      _queryResultPrinter.printCurrencies(currenciesPerExchange);
      break;
    }
    case CoincenterCommandType::kMarkets: {
      const auto marketsPerExchange = getMarketsPerExchange(cmd.cur1(), cmd.cur2(), cmd.exchangeNames());
      _queryResultPrinter.printMarkets(cmd.cur1(), cmd.cur2(), marketsPerExchange, cmd.type());
      break;
    }
    case CoincenterCommandType::kConversion: {
      const auto conversionPerExchange = getConversion(cmd.amount(), cmd.cur1(), cmd.exchangeNames());
      _queryResultPrinter.printConversion(cmd.amount(), cmd.cur1(), conversionPerExchange);
      break;
    }
    case CoincenterCommandType::kConversionPath: {
      const auto conversionPathPerExchange = getConversionPaths(cmd.market(), cmd.exchangeNames());
      _queryResultPrinter.printConversionPath(cmd.market(), conversionPathPerExchange);
      break;
    }
    case CoincenterCommandType::kLastPrice: {
      const auto lastPricePerExchange = getLastPricePerExchange(cmd.market(), cmd.exchangeNames());
      _queryResultPrinter.printLastPrice(cmd.market(), lastPricePerExchange);
      break;
    }
    case CoincenterCommandType::kTicker: {
      const auto exchangeTickerMaps = getTickerInformation(cmd.exchangeNames());
      _queryResultPrinter.printTickerInformation(exchangeTickerMaps);
      break;
    }
    case CoincenterCommandType::kOrderbook: {
      const auto marketOrderBooksConversionRates =
          getMarketOrderBooks(cmd.market(), cmd.exchangeNames(), cmd.cur1(), cmd.optDepth());
      _queryResultPrinter.printMarketOrderBooks(cmd.market(), cmd.cur1(), cmd.optDepth(),
                                                marketOrderBooksConversionRates);
      break;
    }
    case CoincenterCommandType::kLastTrades: {
      const auto lastTradesPerExchange = getLastTradesPerExchange(cmd.market(), cmd.exchangeNames(), cmd.optDepth());
      _queryResultPrinter.printLastTrades(cmd.market(), cmd.optDepth(), lastTradesPerExchange);
      break;
    }
    case CoincenterCommandType::kLast24hTradedVolume: {
      const auto tradedVolumePerExchange = getLast24hTradedVolumePerExchange(cmd.market(), cmd.exchangeNames());
      _queryResultPrinter.printLast24hTradedVolume(cmd.market(), tradedVolumePerExchange);
      break;
    }
    case CoincenterCommandType::kWithdrawFees: {
      const auto withdrawFeesPerExchange = getWithdrawFees(cmd.cur1(), cmd.exchangeNames());
      _queryResultPrinter.printWithdrawFees(withdrawFeesPerExchange, cmd.cur1());
      break;
    }

    case CoincenterCommandType::kBalance: {
      const auto amountIncludePolicy = cmd.withBalanceInUse() ? BalanceOptions::AmountIncludePolicy::kWithBalanceInUse
                                                              : BalanceOptions::AmountIncludePolicy::kOnlyAvailable;
      const BalanceOptions balanceOptions(amountIncludePolicy, cmd.cur1());
      const auto balancePerExchange = getBalance(cmd.exchangeNames(), balanceOptions);
      _queryResultPrinter.printBalance(balancePerExchange, cmd.cur1());
      break;
    }
    case CoincenterCommandType::kDepositInfo: {
      const auto walletPerExchange = getDepositInfo(cmd.exchangeNames(), cmd.cur1());
      _queryResultPrinter.printDepositInfo(cmd.cur1(), walletPerExchange);
      break;
    }
    case CoincenterCommandType::kOrdersClosed: {
      const auto closedOrdersPerExchange = getClosedOrders(cmd.exchangeNames(), cmd.ordersConstraints());
      _queryResultPrinter.printClosedOrders(closedOrdersPerExchange, cmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::kOrdersOpened: {
      const auto openedOrdersPerExchange = getOpenedOrders(cmd.exchangeNames(), cmd.ordersConstraints());
      _queryResultPrinter.printOpenedOrders(openedOrdersPerExchange, cmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::kOrdersCancel: {
      const auto nbCancelledOrdersPerExchange = cancelOrders(cmd.exchangeNames(), cmd.ordersConstraints());
      _queryResultPrinter.printCancelledOrders(nbCancelledOrdersPerExchange, cmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::kRecentDeposits: {
      const auto depositsPerExchange = getRecentDeposits(cmd.exchangeNames(), cmd.withdrawsOrDepositsConstraints());
      _queryResultPrinter.printRecentDeposits(depositsPerExchange, cmd.withdrawsOrDepositsConstraints());
      break;
    }
    case CoincenterCommandType::kRecentWithdraws: {
      const auto withdrawsPerExchange = getRecentWithdraws(cmd.exchangeNames(), cmd.withdrawsOrDepositsConstraints());
      _queryResultPrinter.printRecentWithdraws(withdrawsPerExchange, cmd.withdrawsOrDepositsConstraints());
      break;
    }
    case CoincenterCommandType::kTrade: {
      // 2 input styles are possible:
      //  - standard full information with an amount to trade, a destination currency and an optional list of exchanges
      //  where to trade
      //  - a currency - the destination one, and start amount and exchange(s) should come from previous command result
      auto [startAmount, exchangeNames] = ComputeTradeAmountAndExchanges(cmd, previousTransferableResults);
      if (startAmount.isDefault()) {
        break;
      }
      const auto tradeResultPerExchange =
          trade(startAmount, cmd.isPercentageAmount(), cmd.cur1(), exchangeNames, cmd.tradeOptions());
      _queryResultPrinter.printTrades(tradeResultPerExchange, startAmount, cmd.isPercentageAmount(), cmd.cur1(),
                                      cmd.tradeOptions());
      FillTransferableCommandResults(tradeResultPerExchange, transferableResults);
      break;
    }
    case CoincenterCommandType::kBuy: {
      const auto tradeResultPerExchange = smartBuy(cmd.amount(), cmd.exchangeNames(), cmd.tradeOptions());
      _queryResultPrinter.printBuyTrades(tradeResultPerExchange, cmd.amount(), cmd.tradeOptions());
      FillTransferableCommandResults(tradeResultPerExchange, transferableResults);
      break;
    }
    case CoincenterCommandType::kSell: {
      auto [startAmount, exchangeNames] = ComputeTradeAmountAndExchanges(cmd, previousTransferableResults);
      if (startAmount.isDefault()) {
        break;
      }
      const auto tradeResultPerExchange =
          smartSell(startAmount, cmd.isPercentageAmount(), exchangeNames, cmd.tradeOptions());
      _queryResultPrinter.printSellTrades(tradeResultPerExchange, cmd.amount(), cmd.isPercentageAmount(),
                                          cmd.tradeOptions());
      FillTransferableCommandResults(tradeResultPerExchange, transferableResults);
      break;
    }
    case CoincenterCommandType::kWithdrawApply: {
      const auto [grossAmount, exchangeName] = ComputeWithdrawAmount(cmd, previousTransferableResults);
      if (grossAmount.isDefault()) {
        break;
      }
      const auto deliveredWithdrawInfoWithExchanges = withdraw(grossAmount, cmd.isPercentageAmount(), exchangeName,
                                                               cmd.exchangeNames().back(), cmd.withdrawOptions());
      _queryResultPrinter.printWithdraw(deliveredWithdrawInfoWithExchanges, cmd.isPercentageAmount(),
                                        cmd.withdrawOptions());
      transferableResults.emplace_back(deliveredWithdrawInfoWithExchanges.first[1]->createExchangeName(),
                                       deliveredWithdrawInfoWithExchanges.second.receivedAmount());
      break;
    }
    case CoincenterCommandType::kDustSweeper: {
      _queryResultPrinter.printDustSweeper(dustSweeper(cmd.exchangeNames(), cmd.cur1()), cmd.cur1());
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

void Coincenter::updateFileCaches() const {
  log::debug("Store all cache files");

  _commonAPI.updateCacheFile();
  _fiatConverter.updateCacheFile();

  std::ranges::for_each(_exchangePool.exchanges(), [](const Exchange &exchange) { exchange.updateCacheFile(); });
}

}  // namespace cct
