#include "metricsexporter.hpp"

#include <array>

#include "abstractmetricgateway.hpp"

#define RETURN_IF_NO_MONITORING \
  if (!_pMetricsGateway) return

namespace cct {
void MetricsExporter::exportBalanceMetrics(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency) {
  RETURN_IF_NO_MONITORING;
  MetricKey key = CreateMetricKey("available_balance", "Available balance in the exchange account");
  for (const auto &[exchangePtr, balancePortfolio] : balancePerExchange) {
    const Exchange &exchange = *exchangePtr;
    key.set("exchange", exchange.name());
    key.set("account", exchange.keyName());
    key.set("total", "no");
    MonetaryAmount totalEquiAmount(0, equiCurrency);
    for (BalancePortfolio::MonetaryAmountWithEquivalent amountWithEqui : balancePortfolio) {
      key.set("currency", amountWithEqui.amount.currencyStr());
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key, amountWithEqui.amount.toDouble());
      if (!equiCurrency.isNeutral()) {
        totalEquiAmount += amountWithEqui.equi;
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
  for (const auto &[e, marketOrderBookMap] : marketOrderBookMaps) {
    key.set(kMetricNameKey, "limit_price");
    key.set(kMetricHelpKey, "Best bids and asks prices");
    key.set("exchange", e->name());
    for (const auto &[m, marketOrderbook] : marketOrderBookMap) {
      key.set("market", m.assetsPairStr('-', true));
      key.set("side", "ask");
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                            marketOrderbook.lowestAskPrice().toDouble());
      key.set("side", "bid");
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                            marketOrderbook.highestBidPrice().toDouble());
    }
    key.set(kMetricNameKey, "limit_volume");
    key.set(kMetricHelpKey, "Best bids and asks volumes");
    for (const auto &[m, marketOrderbook] : marketOrderBookMap) {
      key.set("market", m.assetsPairStr('-', true));
      key.set("side", "ask");
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                            marketOrderbook.amountAtAskPrice().toDouble());
      key.set("side", "bid");
      _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                            marketOrderbook.amountAtBidPrice().toDouble());
    }
  }
}

void MetricsExporter::exportOrderbookMetrics(Market m,
                                             const MarketOrderBookConversionRates &marketOrderBookConversionRates) {
  RETURN_IF_NO_MONITORING;
  MetricKey key = CreateMetricKey("limit_pri", "Best bids and asks prices");
  string marketLowerCase = m.assetsPairStr('-', true);
  key.append("market", marketLowerCase);
  for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBookConversionRates) {
    key.set("exchange", exchangeName);
    key.set("side", "ask");
    _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderBook.lowestAskPrice().toDouble());
    key.set("side", "bid");
    _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderBook.highestBidPrice().toDouble());
  }
  key.set(kMetricNameKey, "limit_vol");
  key.set(kMetricHelpKey, "Best bids and asks volumes");
  for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBookConversionRates) {
    key.set("exchange", exchangeName);
    key.set("side", "ask");
    _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                          marketOrderBook.amountAtAskPrice().toDouble());
    key.set("side", "bid");
    _pMetricsGateway->add(MetricType::kGauge, MetricOperation::kSet, key,
                          marketOrderBook.amountAtBidPrice().toDouble());
  }
}

void MetricsExporter::exportLastTradesMetrics(Market m, const LastTradesPerExchange &lastTradesPerExchange) {
  RETURN_IF_NO_MONITORING;
  MetricKey key = CreateMetricKey("", "All public trades that occurred on the market");
  string marketLowerCase = m.assetsPairStr('-', true);
  key.append("market", marketLowerCase);

  for (const auto &[e, lastTrades] : lastTradesPerExchange) {
    key.set("exchange", e->name());

    std::array<MonetaryAmount, 2> totalAmounts{MonetaryAmount(0, m.base()), MonetaryAmount(0, m.base())};
    std::array<MonetaryAmount, 2> totalPrices{MonetaryAmount(0, m.quote()), MonetaryAmount(0, m.quote())};
    std::array<int, 2> nb{};
    for (const PublicTrade &trade : lastTrades) {
      const int buyOrSell = trade.type() == PublicTrade::Type::kBuy ? 0 : 1;

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
}  // namespace cct