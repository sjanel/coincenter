#pragma once

#include <span>

#include "metric.hpp"
#include "monitoringinfo.hpp"

namespace cct {

class AbstractMetricGateway {
 public:
  using BucketBoundaries = std::span<const double>;

  virtual ~AbstractMetricGateway() {}

  /// Register some numeric value
  virtual void add(MetricType metricType, MetricOperation op, const MetricKey& key, double v = 0) = 0;

  /// Create a histogram. Should be called only once
  virtual void createHistogram(const MetricKey& key, BucketBoundaries buckets) = 0;

  /// Create a summary. Should be called only once
  virtual void createSummary(const MetricKey& key, const MetricSummaryInfo& metricSummaryInfo) = 0;

 protected:
  explicit AbstractMetricGateway(const MonitoringInfo& monitoringInfo) : _monitoringInfo(monitoringInfo) {}

  const MonitoringInfo& _monitoringInfo;
};
}  // namespace cct