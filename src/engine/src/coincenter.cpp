#include "coincenter.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

#include "abstractmetricgateway.hpp"
#include "coincentercommands.hpp"
#include "coincenteroptions.hpp"
#include "printqueryresults.hpp"
#include "stringoptionparser.hpp"

namespace cct {

Coincenter::Coincenter(const CoincenterInfo &coincenterInfo, const ExchangeSecretsInfo &exchangeSecretsInfo)
    : _coincenterInfo(coincenterInfo),
      _cryptowatchAPI(_coincenterInfo, coincenterInfo.getRunMode()),
      _fiatConverter(_coincenterInfo, _coincenterInfo.fiatConversionQueryRate()),
      _apiKeyProvider(coincenterInfo.dataDir(), exchangeSecretsInfo, coincenterInfo.getRunMode()),
      _metricsExporter(_coincenterInfo.metricGatewayPtr()),
      _exchangePool(_coincenterInfo, _fiatConverter, _cryptowatchAPI, _apiKeyProvider),
      _exchangesOrchestrator(_exchangePool.exchanges()),
      _queryResultPrinter(_coincenterInfo.printResults()) {}

void Coincenter::process(const CoincenterCommands &opts) {
  processWriteRequests(opts);
  const int nbRepeats = opts.repeats;
  for (int repeatPos = 0; repeatPos != nbRepeats; ++repeatPos) {
    if (repeatPos != 0) {
      std::this_thread::sleep_for(opts.repeatTime);
    }
    if (nbRepeats != 1) {
      if (nbRepeats == -1) {
        log::info("Processing read request {}/{}", repeatPos + 1, "\u221E");  // unicode char for infinity sign
      } else {
        log::info("Processing read request {}/{}", repeatPos + 1, nbRepeats);
      }
    }

    processReadRequests(opts);
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

void Coincenter::cancelOrders(std::span<const ExchangeName> privateExchangeNames,
                              const OrdersConstraints &ordersConstraints) {
  _exchangesOrchestrator.cancelOrders(privateExchangeNames, ordersConstraints);
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

TradedAmounts Coincenter::trade(MonetaryAmount startAmount, bool isPercentageTrade, CurrencyCode toCurrency,
                                std::span<const ExchangeName> privateExchangeNames, const TradeOptions &tradeOptions) {
  return _exchangesOrchestrator.trade(startAmount, isPercentageTrade, toCurrency, privateExchangeNames, tradeOptions);
}

Coincenter::TradedAmountsVector Coincenter::smartBuy(MonetaryAmount endAmount,
                                                     std::span<const ExchangeName> privateExchangeNames,
                                                     const TradeOptions &tradeOptions) {
  return _exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions);
}

Coincenter::TradedAmountsVector Coincenter::smartSell(MonetaryAmount startAmount, bool isPercentageTrade,
                                                      std::span<const ExchangeName> privateExchangeNames,
                                                      const TradeOptions &tradeOptions) {
  return _exchangesOrchestrator.smartSell(startAmount, isPercentageTrade, privateExchangeNames, tradeOptions);
}

TradedAmounts Coincenter::tradeAll(CurrencyCode fromCurrency, CurrencyCode toCurrency,
                                   std::span<const ExchangeName> privateExchangeNames,
                                   const TradeOptions &tradeOptions) {
  return _exchangesOrchestrator.tradeAll(fromCurrency, toCurrency, privateExchangeNames, tradeOptions);
}

WithdrawInfo Coincenter::withdraw(MonetaryAmount grossAmount, bool isPercentageWithdraw,
                                  const ExchangeName &fromPrivateExchangeName,
                                  const ExchangeName &toPrivateExchangeName) {
  return _exchangesOrchestrator.withdraw(grossAmount, isPercentageWithdraw, fromPrivateExchangeName,
                                         toPrivateExchangeName);
}

WithdrawFeePerExchange Coincenter::getWithdrawFees(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames) {
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

void Coincenter::processReadRequests(const CoincenterCommands &opts) {
  if (!opts.marketsCurrency1.isNeutral()) {
    MarketsPerExchange marketsPerExchange =
        getMarketsPerExchange(opts.marketsCurrency1, opts.marketsCurrency2, opts.marketsExchanges);
    _queryResultPrinter.printMarkets(opts.marketsCurrency1, opts.marketsCurrency2, marketsPerExchange);
  }

  if (opts.tickerForAll || !opts.tickerExchanges.empty()) {
    ExchangeTickerMaps exchangeTickerMaps = getTickerInformation(opts.tickerExchanges);
    _queryResultPrinter.printTickerInformation(exchangeTickerMaps);
  }

  if (!opts.marketForOrderBook.isNeutral()) {
    std::optional<int> depth;
    if (opts.orderbookDepth != 0) {
      depth = opts.orderbookDepth;
    }
    MarketOrderBookConversionRates marketOrderBooksConversionRates =
        getMarketOrderBooks(opts.marketForOrderBook, opts.orderBookExchanges, opts.orderbookCur, depth);
    _queryResultPrinter.printMarketOrderBooks(marketOrderBooksConversionRates);
  }

  if (!opts.marketForConversionPath.isNeutral()) {
    ConversionPathPerExchange conversionPathPerExchange =
        getConversionPaths(opts.marketForConversionPath, opts.conversionPathExchanges);
    _queryResultPrinter.printConversionPath(opts.marketForConversionPath, conversionPathPerExchange);
  }

  if (opts.balanceForAll || !opts.balancePrivateExchanges.empty()) {
    BalancePerExchange balancePerExchange = getBalance(opts.balancePrivateExchanges, opts.balanceCurrencyCode);
    _queryResultPrinter.printBalance(balancePerExchange);
  }

  if (!opts.depositCurrency.isNeutral()) {
    WalletPerExchange walletPerExchange = getDepositInfo(opts.depositInfoPrivateExchanges, opts.depositCurrency);

    _queryResultPrinter.printDepositInfo(opts.depositCurrency, walletPerExchange);
  }

  if (opts.queryOpenedOrders) {
    OpenedOrdersPerExchange openedOrdersPerExchange =
        getOpenedOrders(opts.openedOrdersPrivateExchanges, opts.openedOrdersConstraints);

    _queryResultPrinter.printOpenedOrders(openedOrdersPerExchange);
  }

  if (opts.cancelOpenedOrders) {
    cancelOrders(opts.cancelOpenedOrdersPrivateExchanges, opts.cancelOpenedOrdersConstraints);
  }

  if (!opts.withdrawFeeCur.isNeutral()) {
    auto withdrawFeesPerExchange = getWithdrawFees(opts.withdrawFeeCur, opts.withdrawFeeExchanges);
    _queryResultPrinter.printWithdrawFees(withdrawFeesPerExchange);
  }

  if (!opts.tradedVolumeMarket.isNeutral()) {
    MonetaryAmountPerExchange tradedVolumePerExchange =
        getLast24hTradedVolumePerExchange(opts.tradedVolumeMarket, opts.tradedVolumeExchanges);
    _queryResultPrinter.printLast24hTradedVolume(opts.tradedVolumeMarket, tradedVolumePerExchange);
  }

  if (!opts.lastTradesMarket.isNeutral()) {
    LastTradesPerExchange lastTradesPerExchange =
        getLastTradesPerExchange(opts.lastTradesMarket, opts.lastTradesExchanges, opts.nbLastTrades);
    _queryResultPrinter.printLastTrades(opts.lastTradesMarket, lastTradesPerExchange);
  }

  if (!opts.lastPriceMarket.isNeutral()) {
    MonetaryAmountPerExchange lastPricePerExchange =
        getLastPricePerExchange(opts.lastPriceMarket, opts.lastPriceExchanges);
    _queryResultPrinter.printLastPrice(opts.lastPriceMarket, lastPricePerExchange);
  }
}

void Coincenter::processWriteRequests(const CoincenterCommands &opts) {
  if (!opts.fromTradeCurrency.isNeutral() && !opts.toTradeCurrency.isNeutral()) {
    tradeAll(opts.fromTradeCurrency, opts.toTradeCurrency, opts.tradePrivateExchangeNames, opts.tradeOptions);
  }

  if (!opts.endTradeAmount.isZero()) {
    smartBuy(opts.endTradeAmount, opts.tradePrivateExchangeNames, opts.tradeOptions);
  } else if (!opts.startTradeAmount.isZero()) {
    if (opts.toTradeCurrency.isNeutral()) {
      smartSell(opts.startTradeAmount, opts.isPercentageTrade, opts.tradePrivateExchangeNames, opts.tradeOptions);
    } else {
      trade(opts.startTradeAmount, opts.isPercentageTrade, opts.toTradeCurrency, opts.tradePrivateExchangeNames,
            opts.tradeOptions);
    }
  }

  if (!opts.amountToWithdraw.isZero()) {
    withdraw(opts.amountToWithdraw, opts.isPercentageWithdraw, opts.withdrawFromExchangeName,
             opts.withdrawToExchangeName);
  }
}

}  // namespace cct
