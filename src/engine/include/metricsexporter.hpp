#pragma once

#include "currencycode.hpp"
#include "queryresulttypes.hpp"

namespace cct {

class AbstractMetricGateway;

class MetricsExporter {
 public:
  explicit MetricsExporter(AbstractMetricGateway *pMetricsGateway);

  void exportHealthCheckMetrics(const ExchangeHealthCheckStatus &healthCheckPerExchange);

  void exportBalanceMetrics(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency);

  void exportTickerMetrics(const ExchangeTickerMaps &marketOrderBookMaps);

  void exportOrderbookMetrics(const MarketOrderBookConversionRates &marketOrderBookConversionRates);

  void exportLastTradesMetrics(const TradesPerExchange &lastTradesPerExchange);

 private:
  void createSummariesAndHistograms();

  AbstractMetricGateway *_pMetricsGateway;
};
}  // namespace cct