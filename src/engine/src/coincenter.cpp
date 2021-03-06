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
      _cryptowatchAPI(_coincenterInfo, coincenterInfo.getRunMode()),
      _fiatConverter(_coincenterInfo, _coincenterInfo.fiatConversionQueryRate()),
      _apiKeyProvider(coincenterInfo.dataDir(), exchangeSecretsInfo, coincenterInfo.getRunMode()),
      _metricsExporter(_coincenterInfo.metricGatewayPtr()),
      _exchangePool(_coincenterInfo, _fiatConverter, _cryptowatchAPI, _apiKeyProvider),
      _exchangesOrchestrator(_exchangePool.exchanges()),
      _queryResultPrinter(std::cout, _coincenterInfo.apiOutputType()) {}

void Coincenter::process(const CoincenterCommands &coincenterCommands) {
  const int nbRepeats = coincenterCommands.repeats();
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
    for (const CoincenterCommand &cmd : coincenterCommands.commands()) {
      processCommand(cmd);
    }
  }
}

void Coincenter::processCommand(const CoincenterCommand &cmd) {
  switch (cmd.type()) {
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
      BalancePerExchange balancePerExchange = getBalance(cmd.exchangeNames(), cmd.cur1());
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
      WithdrawInfo withdrawInfo =
          withdraw(cmd.amount(), cmd.isPercentageAmount(), cmd.exchangeNames().front(), cmd.exchangeNames().back());
      _queryResultPrinter.printWithdraw(withdrawInfo, cmd.amount(), cmd.isPercentageAmount(),
                                        cmd.exchangeNames().front(), cmd.exchangeNames().back());
      break;
    }
    default:
      throw exception("Unknown command type");
  }
}

ExchangeTickerMaps Coincenter::getTickerInformation(ExchangeNameSpan exchangeNames) {
  ExchangeTickerMaps ret = _exchangesOrchestrator.getTickerInformation(exchangeNames);

  _metricsExporter.exportTickerMetrics(ret);

  return ret;
}

MarketOrderBookConversionRates Coincenter::getMarketOrderBooks(Market m, ExchangeNameSpan exchangeNames,
                                                               CurrencyCode equiCurrencyCode,
                                                               std::optional<int> depth) {
  MarketOrderBookConversionRates ret =
      _exchangesOrchestrator.getMarketOrderBooks(m, exchangeNames, equiCurrencyCode, depth);

  _metricsExporter.exportOrderbookMetrics(m, ret);

  return ret;
}

BalancePerExchange Coincenter::getBalance(std::span<const ExchangeName> privateExchangeNames,
                                          CurrencyCode equiCurrency) {
  std::optional<CurrencyCode> optEquiCur = _coincenterInfo.fiatCurrencyIfStableCoin(equiCurrency);
  if (optEquiCur) {
    log::warn("Consider {} instead of stable coin {} as equivalent currency", optEquiCur->str(), equiCurrency.str());
    equiCurrency = *optEquiCur;
  }

  BalancePerExchange ret = _exchangesOrchestrator.getBalance(privateExchangeNames, equiCurrency);

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

ConversionPathPerExchange Coincenter::getConversionPaths(Market m, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getConversionPaths(m, exchangeNames);
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

UniquePublicSelectedExchanges Coincenter::getExchangesTradingMarket(Market m, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getExchangesTradingMarket(m, exchangeNames);
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

WithdrawInfo Coincenter::withdraw(MonetaryAmount grossAmount, bool isPercentageWithdraw,
                                  const ExchangeName &fromPrivateExchangeName,
                                  const ExchangeName &toPrivateExchangeName) {
  return _exchangesOrchestrator.withdraw(grossAmount, isPercentageWithdraw, fromPrivateExchangeName,
                                         toPrivateExchangeName);
}

MonetaryAmountPerExchange Coincenter::getWithdrawFees(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getWithdrawFees(currencyCode, exchangeNames);
}

MonetaryAmountPerExchange Coincenter::getLast24hTradedVolumePerExchange(Market m, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getLast24hTradedVolumePerExchange(m, exchangeNames);
}

LastTradesPerExchange Coincenter::getLastTradesPerExchange(Market m, ExchangeNameSpan exchangeNames, int nbLastTrades) {
  LastTradesPerExchange ret = _exchangesOrchestrator.getLastTradesPerExchange(m, exchangeNames, nbLastTrades);

  _metricsExporter.exportLastTradesMetrics(m, ret);

  return ret;
}

MonetaryAmountPerExchange Coincenter::getLastPricePerExchange(Market m, ExchangeNameSpan exchangeNames) {
  return _exchangesOrchestrator.getLastPricePerExchange(m, exchangeNames);
}

void Coincenter::updateFileCaches() const {
  log::debug("Store all cache files");
  _cryptowatchAPI.updateCacheFile();
  _fiatConverter.updateCacheFile();
  auto exchanges = _exchangePool.exchanges();
  std::for_each(exchanges.begin(), exchanges.end(), [](const Exchange &e) { e.updateCacheFile(); });
}

}  // namespace cct
