#include "coincenter.hpp"

#include <algorithm>
#include <optional>
#include <span>
#include <thread>

#include "balanceoptions.hpp"
#include "cct_exception.hpp"
#include "coincentercommands.hpp"
#include "coincentercommandtype.hpp"
#include "coincenterinfo.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
#include "exchangename.hpp"
#include "exchangeretriever.hpp"
#include "exchangesecretsinfo.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "queryresultprinter.hpp"
#include "queryresulttypes.hpp"
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
      _exchangesOrchestrator(coincenterInfo.requestsConfig(), _exchangePool.exchanges()),
      _queryResultPrinter(coincenterInfo.apiOutputType(), _coincenterInfo.loggingInfo()) {}

int Coincenter::process(const CoincenterCommands &coincenterCommands) {
  int nbCommandsProcessed = 0;
  const auto commands = coincenterCommands.commands();
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
    for (const auto &cmd : commands) {
      processCommand(cmd);
      ++nbCommandsProcessed;
    }
  }
  return nbCommandsProcessed;
}

void Coincenter::processCommand(const CoincenterCommand &cmd) {
  switch (cmd.type()) {
    case CoincenterCommandType::kHealthCheck: {
      const auto healthCheckStatus = healthCheck(cmd.exchangeNames());
      _queryResultPrinter.printHealthCheck(healthCheckStatus);
      break;
    }
    case CoincenterCommandType::kMarkets: {
      const auto marketsPerExchange = getMarketsPerExchange(cmd.cur1(), cmd.cur2(), cmd.exchangeNames());
      _queryResultPrinter.printMarkets(cmd.cur1(), cmd.cur2(), marketsPerExchange);
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
      const auto lastTradesPerExchange =
          getLastTradesPerExchange(cmd.market(), cmd.exchangeNames(), cmd.nbLastTrades());
      _queryResultPrinter.printLastTrades(cmd.market(), cmd.nbLastTrades(), lastTradesPerExchange);
      break;
    }
    case CoincenterCommandType::kLast24hTradedVolume: {
      const auto tradedVolumePerExchange = getLast24hTradedVolumePerExchange(cmd.market(), cmd.exchangeNames());
      _queryResultPrinter.printLast24hTradedVolume(cmd.market(), tradedVolumePerExchange);
      break;
    }
    case CoincenterCommandType::kWithdrawFee: {
      const auto withdrawFeesPerExchange = getWithdrawFees(cmd.cur1(), cmd.exchangeNames());
      _queryResultPrinter.printWithdrawFees(withdrawFeesPerExchange, cmd.cur1());
      break;
    }

    case CoincenterCommandType::kBalance: {
      const BalanceOptions balanceOptions(cmd.withBalanceInUse()
                                              ? BalanceOptions::AmountIncludePolicy::kWithBalanceInUse
                                              : BalanceOptions::AmountIncludePolicy::kOnlyAvailable,
                                          cmd.cur1());
      const auto balancePerExchange = getBalance(cmd.exchangeNames(), balanceOptions);
      _queryResultPrinter.printBalance(balancePerExchange, cmd.cur1());
      break;
    }
    case CoincenterCommandType::kDepositInfo: {
      const auto walletPerExchange = getDepositInfo(cmd.exchangeNames(), cmd.cur1());
      _queryResultPrinter.printDepositInfo(cmd.cur1(), walletPerExchange);
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
      const auto tradeResultPerExchange =
          trade(cmd.amount(), cmd.isPercentageAmount(), cmd.cur1(), cmd.exchangeNames(), cmd.tradeOptions());
      _queryResultPrinter.printTrades(tradeResultPerExchange, cmd.amount(), cmd.isPercentageAmount(), cmd.cur1(),
                                      cmd.tradeOptions());
      break;
    }
    case CoincenterCommandType::kBuy: {
      const auto tradeResultPerExchange = smartBuy(cmd.amount(), cmd.exchangeNames(), cmd.tradeOptions());
      _queryResultPrinter.printBuyTrades(tradeResultPerExchange, cmd.amount(), cmd.tradeOptions());
      break;
    }
    case CoincenterCommandType::kSell: {
      const auto tradeResultPerExchange =
          smartSell(cmd.amount(), cmd.isPercentageAmount(), cmd.exchangeNames(), cmd.tradeOptions());
      _queryResultPrinter.printSellTrades(tradeResultPerExchange, cmd.amount(), cmd.isPercentageAmount(),
                                          cmd.tradeOptions());
      break;
    }
    case CoincenterCommandType::kWithdraw: {
      const auto &fromExchangeName = cmd.exchangeNames().front();
      const auto &toExchangeName = cmd.exchangeNames().back();
      const auto deliveredWithdrawInfoWithExchanges =
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
  const auto optEquiCur = _coincenterInfo.fiatCurrencyIfStableCoin(equiCurrency);
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
  _commonAPI.updateCacheFile();
  _fiatConverter.updateCacheFile();
  std::ranges::for_each(_exchangePool.exchanges(), [](const Exchange &exchange) { exchange.updateCacheFile(); });
}

}  // namespace cct
