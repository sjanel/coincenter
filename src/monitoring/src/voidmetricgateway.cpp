#include "voidmetricgateway.hpp"

#include "cct_log.hpp"

namespace cct {
namespace {
void LogError(std::string_view info) {
  log::error("Trying to {} without a gateway - nothing is done", info);
  log::error("Consider recompiling program with client implementation");
}
}  // namespace

void VoidMetricGateway::add([[maybe_unused]] MetricType metricType, [[maybe_unused]] MetricOperation op,
                            [[maybe_unused]] const MetricKey &key, [[maybe_unused]] double val) {
  LogError("register metric");
}

void VoidMetricGateway::createHistogram([[maybe_unused]] const MetricKey &key,
                                        [[maybe_unused]] BucketBoundaries buckets) {
  LogError("create a Histogram metric");
}

void VoidMetricGateway::createSummary([[maybe_unused]] const MetricKey &key,
                                      [[maybe_unused]] const MetricSummaryInfo &metricSummaryInfo) {
  LogError("create a Summary metric");
}
}  // namespace cct