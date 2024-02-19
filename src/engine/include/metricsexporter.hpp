#pragma once

#include "currencycode.hpp"
#include "market.hpp"
#include "queryresulttypes.hpp"

namespace cct {

class AbstractMetricGateway;

class MetricsExporter {
 public:
  explicit MetricsExporter(AbstractMetricGateway *pMetricsGateway);

  void exportHealthCheckMetrics(const ExchangeHealthCheckStatus &healthCheckPerExchange);

  void exportBalanceMetrics(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency);

  void exportTickerMetrics(const ExchangeTickerMaps &marketOrderBookMaps);

  void exportOrderbookMetrics(Market mk, const MarketOrderBookConversionRates &marketOrderBookConversionRates);

  void exportLastTradesMetrics(Market mk, const TradesPerExchange &lastTradesPerExchange);

 private:
  void createSummariesAndHistograms();

  AbstractMetricGateway *_pMetricsGateway;
};
}  // namespace cct