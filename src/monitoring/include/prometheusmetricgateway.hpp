#pragma once

#include <prometheus/gateway.h>
#include <prometheus/registry.h>

#include <memory>
#include <mutex>
#include <unordered_map>

#include "abstractmetricgateway.hpp"
#include "timedef.hpp"

/// Registry to collect stats
namespace cct {

/// High performance Prometheus gateway implementation, caching the metrics as they come along in a HashMap.
class PrometheusMetricGateway : public AbstractMetricGateway {
 public:
  using Gateway = prometheus::Gateway;
  using Registry = prometheus::Registry;

  explicit PrometheusMetricGateway(const MonitoringInfo &monitoringInfo);

  PrometheusMetricGateway(const PrometheusMetricGateway &) = delete;
  PrometheusMetricGateway(PrometheusMetricGateway &&) = delete;
  PrometheusMetricGateway &operator=(const PrometheusMetricGateway &) = delete;
  PrometheusMetricGateway &operator=(PrometheusMetricGateway &&) = delete;

  ~PrometheusMetricGateway() override;

  void add(MetricType metricType, MetricOperation op, const MetricKey &key, double val = 0) override;

  void createHistogram(const MetricKey &key, BucketBoundaries buckets) override;

  void createSummary(const MetricKey &key, const MetricSummaryInfo &metricSummaryInfo) override;

 private:
  void flush();
  void checkFlush();

  Gateway _gateway;
  std::shared_ptr<Registry> _registry;
  std::unordered_map<MetricKey, void *> _familiesMap;
  std::mutex _familiesMapMutex;
  TimePoint _lastFlushedTime;
  int _checkFlushCounter{};  // To decrease number of times flush check is done
};
}  // namespace cct
