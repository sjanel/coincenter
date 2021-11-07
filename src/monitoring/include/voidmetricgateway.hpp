#pragma once

#include <string_view>

#include "abstractmetricgateway.hpp"

namespace cct {
class VoidMetricGateway : public AbstractMetricGateway {
 public:
  explicit VoidMetricGateway(const MonitoringInfo &monitoringInfo) : AbstractMetricGateway(monitoringInfo) {}

  void add(MetricType metricType, MetricOperation op, const MetricKey &key, double v = 0) override;

  void createHistogram(const MetricKey &key, BucketBoundaries buckets) override;

  void createSummary(const MetricKey &key, const MetricSummaryInfo &metricSummaryInfo) override;
};
}  // namespace cct