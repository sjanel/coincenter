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

void Coincenter::process(const CoincenterCommands &coincenterCommands) {
  processWriteRequests(coincenterCommands);
  const int nbRepeats = coincenterCommands.repeats;
  for (int repeatPos = 0; repeatPos != nbRepeats; ++repeatPos) {
    if (repeatPos != 0) {
      std::this_thread::sleep_for(coincenterCommands.repeatTime);
    }
    if (nbRepeats != 1) {
      if (nbRepeats == -1) {
        log::info("Processing read request {}/{}", repeatPos + 1, "\u221E");  // unicode char for infinity sign
      } else {
        log::info("Processing read request {}/{}", repeatPos + 1, nbRepeats);
      }
    }
  }

  for (const CoincenterCommand &cmd : coincenterCommands.commands()) {
    switch (cmd.type()) {
      case CoincenterCommand::Type::kMarkets: {
        MarketsPerExchange marketsPerExchange = getMarketsPerExchange(cmd.cur1(), cmd.cur2(), cmd.exchangeNames());
        _queryResultPrinter.printMarkets(cmd.cur1(), cmd.cur2(), marketsPerExchange);
        break;
      }
      case CoincenterCommand::Type::kConversionPath: {
        ConversionPathPerExchange conversionPathPerExchange = getConversionPaths(cmd.market(), cmd.exchangeNames());
        _queryResultPrinter.printConversionPath(cmd.market(), conversionPathPerExchange);
        break;
      }
      case CoincenterCommand::Type::kLastPrice: {
        MonetaryAmountPerExchange lastPricePerExchange = getLastPricePerExchange(cmd.market(), cmd.exchangeNames());
        _queryResultPrinter.printLastPrice(cmd.market(), lastPricePerExchange);
        break;
      }
      case CoincenterCommand::Type::kTicker: {
        ExchangeTickerMaps exchangeTickerMaps = getTickerInformation(cmd.exchangeNames());
        _queryResultPrinter.printTickerInformation(exchangeTickerMaps);
        break;
      }
      case CoincenterCommand::Type::kOrderbook: {
        std::optional<int> depth = cmd.n() != 0 ? std::optional<int>(cmd.n()) : std::nullopt;
        MarketOrderBookConversionRates marketOrderBooksConversionRates =
            getMarketOrderBooks(cmd.market(), cmd.exchangeNames(), cmd.cur1(), depth);
        _queryResultPrinter.printMarketOrderBooks(marketOrderBooksConversionRates);
        break;
      }
      case CoincenterCommand::Type::kLastTrades: {
        LastTradesPerExchange lastTradesPerExchange =
            getLastTradesPerExchange(cmd.market(), cmd.exchangeNames(), cmd.n());
        _queryResultPrinter.printLastTrades(cmd.market(), lastTradesPerExchange);
        break;
      }
      case CoincenterCommand::Type::kLast24hTradedVolume: {
        MonetaryAmountPerExchange tradedVolumePerExchange =
            getLast24hTradedVolumePerExchange(cmd.market(), cmd.exchangeNames());
        _queryResultPrinter.printLast24hTradedVolume(cmd.market(), tradedVolumePerExchange);
        break;
      }
      case CoincenterCommand::Type::kWithdrawFee: {
        auto withdrawFeesPerExchange = getWithdrawFees(cmd.cur1(), cmd.exchangeNames());
        _queryResultPrinter.printWithdrawFees(withdrawFeesPerExchange);
        break;
      }

      case CoincenterCommand::Type::kBalance: {
        BalancePerExchange balancePerExchange = getBalance(cmd.exchangeNames(), cmd.cur1());
        _queryResultPrinter.printBalance(balancePerExchange);
        break;
      }
      case CoincenterCommand::Type::kDepositInfo: {
        WalletPerExchange walletPerExchange = getDepositInfo(cmd.exchangeNames(), cmd.cur1());
        _queryResultPrinter.printDepositInfo(cmd.cur1(), walletPerExchange);
        break;
      }
      case CoincenterCommand::Type::kOrdersOpened: {
        OpenedOrdersPerExchange openedOrdersPerExchange = getOpenedOrders(cmd.exchangeNames(), cmd.ordersConstraints());
        _queryResultPrinter.printOpenedOrders(openedOrdersPerExchange);
        break;
      }
      case CoincenterCommand::Type::kOrdersCancel:
        cancelOrders(cmd.exchangeNames(), cmd.ordersConstraints());
        break;
      case CoincenterCommand::Type::kTrade:
        break;
      case CoincenterCommand::Type::kBuy: {
        smartBuy(cmd.amount(), cmd.exchangeNames(), coincenterCommands.tradeOptions);
        break;
      }
      case CoincenterCommand::Type::kSell:
        break;
      case CoincenterCommand::Type::kWithdraw: {
        withdraw(cmd.amount(), cmd.isPercentageAmount(), cmd.exchangeNames().front(), cmd.exchangeNames().back());
        break;
      }
      default:
        throw exception("Unknown command type");
    }
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

void Coincenter::processWriteRequests(const CoincenterCommands &coincenterCommands) {
  if (!coincenterCommands.fromTradeCurrency.isNeutral() && !coincenterCommands.toTradeCurrency.isNeutral()) {
    tradeAll(coincenterCommands.fromTradeCurrency, coincenterCommands.toTradeCurrency,
             coincenterCommands.tradePrivateExchangeNames, coincenterCommands.tradeOptions);
  }

  if (!coincenterCommands.endTradeAmount.isZero()) {
    smartBuy(coincenterCommands.endTradeAmount, coincenterCommands.tradePrivateExchangeNames,
             coincenterCommands.tradeOptions);
  } else if (!coincenterCommands.startTradeAmount.isZero()) {
    if (coincenterCommands.toTradeCurrency.isNeutral()) {
      smartSell(coincenterCommands.startTradeAmount, coincenterCommands.isPercentageTrade,
                coincenterCommands.tradePrivateExchangeNames, coincenterCommands.tradeOptions);
    } else {
      trade(coincenterCommands.startTradeAmount, coincenterCommands.isPercentageTrade,
            coincenterCommands.toTradeCurrency, coincenterCommands.tradePrivateExchangeNames,
            coincenterCommands.tradeOptions);
    }
  }
}

}  // namespace cct
