#include "voidmetricgateway.hpp"

#include "cct_log.hpp"

namespace cct {
namespace {
void LogError(std::string_view info) {
  log::error("Trying to {} without a gateway - nothing is done", info);
  log::error("Consider recompiling program with client implementation");
}
}  // namespace

void VoidMetricGateway::add(MetricType, MetricOperation, const MetricKey &, double) { LogError("register metric"); }

void VoidMetricGateway::createHistogram(const MetricKey &, BucketBoundaries) { LogError("create a Histogram metric"); }

void VoidMetricGateway::createSummary(const MetricKey &, const MetricSummaryInfo &) {
  LogError("create a Summary metric");
}
}  // namespace cct