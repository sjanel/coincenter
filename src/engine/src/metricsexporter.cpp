#include "metricsexporter.hpp"

#include <array>

#include "abstractmetricgateway.hpp"
#include "curlmetrics.hpp"
#include "currencycode.hpp"
#include "enum-string.hpp"
#include "exchange.hpp"
#include "market.hpp"
#include "metric.hpp"
#include "monetaryamount.hpp"
#include "publictrade.hpp"
#include "queryresulttypes.hpp"
#include "tradeside.hpp"

#define RETURN_IF_NO_MONITORING \
  if (!_pMetricsGateway) return

namespace cct {

MetricsExporter::MetricsExporter(AbstractMetricGateway *pMetricsGateway) : _pMetricsGateway(pMetricsGateway) {
  if (_pMetricsGateway != nullptr) {
    createSummariesAndHistograms();
  }
}

void MetricsExporter::exportHealthCheckMetrics(const ExchangeHealthCheckStatus &healthCheckPerExchange) {
  RETURN_IF_NO_MONITORING;
  MetricKey key = CreateMetricKey("health_check", "Is exchange status OK?");
  for (const auto &[exchangePtr, healthCheckResult] : healthCheckPerExchange) {
    const Exchange &exchange = *exchangePtr;
    key.set("exchange", exchange.name());
    _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key, static_cast<double>(healthCheckResult));
  }
}

void MetricsExporter::exportBalanceMetrics(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency) {
  RETURN_IF_NO_MONITORING;
  MetricKey key = CreateMetricKey("available_balance", "Available balance in the exchange account");
  for (const auto &[exchangePtr, balancePortfolio] : balancePerExchange) {
    const Exchange &exchange = *exchangePtr;
    key.set("exchange", exchange.name());
    key.set("account", exchange.keyName());
    key.set("total", "no");
    MonetaryAmount totalEquiAmount(0, equiCurrency);
    for (auto [amount, equi] : balancePortfolio) {
      key.set("currency", amount.currencyStr());
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key, amount.toDouble());
      if (equiCurrency.isDefined()) {
        totalEquiAmount += equi;
      }
    }
    if (!equiCurrency.isNeutral()) {
      key.set("total", "yes");
      key.set("currency", equiCurrency.str());
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key, totalEquiAmount.toDouble());
    }
  }
}

void MetricsExporter::exportTickerMetrics(const ExchangeTickerMaps &marketOrderBookMaps) {
  RETURN_IF_NO_MONITORING;
  MetricKey key;
  for (const auto &[exchange, marketOrderBookMap] : marketOrderBookMaps) {
    key.set(kMetricNameKey, "limit_price");
    key.set(kMetricHelpKey, "Best bids and asks prices");
    key.set("exchange", exchange->name());
    for (const auto &[mk, marketOrderbook] : marketOrderBookMap) {
      key.set("market", mk.assetsPairStrLower('-'));
      key.set("side", "ask");
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                            marketOrderbook.lowestAskPrice().toDouble());
      key.set("side", "bid");
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                            marketOrderbook.highestBidPrice().toDouble());
    }
    key.set(kMetricNameKey, "limit_volume");
    key.set(kMetricHelpKey, "Best bids and asks volumes");
    for (const auto &[mk, marketOrderbook] : marketOrderBookMap) {
      key.set("market", mk.assetsPairStrLower('-'));
      key.set("side", "ask");
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                            marketOrderbook.amountAtAskPrice().toDouble());
      key.set("side", "bid");
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                            marketOrderbook.amountAtBidPrice().toDouble());
    }
  }
}

void MetricsExporter::exportOrderbookMetrics(const MarketOrderBookConversionRates &marketOrderBookConversionRates) {
  RETURN_IF_NO_MONITORING;
  MetricKey key = CreateMetricKey("limit_pri", "Best bids and asks prices");

  for (const auto &[exchangeNameEnum, marketOrderBook, optConversionRate] : marketOrderBookConversionRates) {
    key.set("market", marketOrderBook.market().assetsPairStrLower('-'));
    key.set("exchange", EnumToString(exchangeNameEnum));
    key.set("side", "ask");
    _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderBook.lowestAskPrice().toDouble());
    key.set("side", "bid");
    _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderBook.highestBidPrice().toDouble());
  }
  key.set(kMetricNameKey, "limit_vol");
  key.set(kMetricHelpKey, "Best bids and asks volumes");
  for (const auto &[exchangeNameEnum, marketOrderBook, optConversionRate] : marketOrderBookConversionRates) {
    key.set("market", marketOrderBook.market().assetsPairStrLower('-'));
    key.set("exchange", EnumToString(exchangeNameEnum));
    key.set("side", "ask");
    _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                          marketOrderBook.amountAtAskPrice().toDouble());
    key.set("side", "bid");
    _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                          marketOrderBook.amountAtBidPrice().toDouble());
  }
}

void MetricsExporter::exportLastTradesMetrics(const TradesPerExchange &lastTradesPerExchange) {
  RETURN_IF_NO_MONITORING;
  MetricKey key = CreateMetricKey("", "All public trades that occurred on the market");

  for (const auto &[exchange, lastTrades] : lastTradesPerExchange) {
    if (lastTrades.empty()) {
      continue;
    }
    Market mk = lastTrades.front().market();
    key.set("market", mk.assetsPairStrLower('-'));
    key.set("exchange", exchange->name());

    std::array<MonetaryAmount, 2> totalAmounts{MonetaryAmount(0, mk.base()), MonetaryAmount(0, mk.base())};
    std::array<MonetaryAmount, 2> totalPrices{MonetaryAmount(0, mk.quote()), MonetaryAmount(0, mk.quote())};
    std::array<int, 2> nb{};
    for (const PublicTrade &trade : lastTrades) {
      const int buyOrSell = trade.side() == TradeSide::buy ? 0 : 1;

      totalAmounts[buyOrSell] += trade.amount();
      ++nb[buyOrSell];
      totalPrices[buyOrSell] += trade.price();
    }
    for (int buyOrSell = 0; buyOrSell < 2; ++buyOrSell) {
      if (nb[buyOrSell] > 0) {
        key.set("side", buyOrSell == 0 ? "buy" : "sell");
        MonetaryAmount avgAmount = totalAmounts[buyOrSell] / nb[buyOrSell];
        MonetaryAmount avgPrice = totalPrices[buyOrSell] / nb[buyOrSell];

        key.set(kMetricNameKey, "public_trade_amount");
        _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key, avgAmount.toDouble());
        key.set(kMetricNameKey, "public_trade_price");
        _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key, avgPrice.toDouble());
      }
    }
  }
}

void MetricsExporter::createSummariesAndHistograms() {
  for (const auto &[requestType, metricKey] : CurlMetrics::kRequestDurationKeys) {
    static constexpr std::array kRequestDurationBoundariesMs = {5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0};
    _pMetricsGateway->createHistogram(metricKey, kRequestDurationBoundariesMs);
  }
}
}  // namespace cct