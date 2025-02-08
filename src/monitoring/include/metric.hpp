#pragma once

#include <cstdint>
#include <string_view>

#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "flatkeyvaluestring.hpp"
#include "timedef.hpp"

namespace cct {
// Metric type (and documentation) is extracted from Prometheus model
enum class MetricType : int8_t {
  kCounter,    // A counter metric to represent a monotonically increasing (not decreasing!) value.
  kGauge,      // A gauge metric to represent a value that can arbitrarily go up and down
  kHistogram,  // A histogram metric to represent aggregatable distributions of events.
  kSummary     // A summary metric samples observations over a sliding window of time.
};

// Controls how the metric is updated
enum class MetricOperation : int8_t { kIncrement, kDecrement, kSetCurrentTime, kSet, kObserve };

// Flattened key value string containing the description and labels of the metric.
// At minimum, it should contain the following keys:
//  - metric_name: name of the metric
//  - metric_help: short helper description of the metric
using MetricKey = FlatKeyValueString<',', '='>;

inline constexpr std::string_view kMetricNameKey = "metric_name";
inline constexpr std::string_view kMetricHelpKey = "metric_help";

MetricKey CreateMetricKey(std::string_view name, std::string_view help);

struct MetricSummaryInfo {
  struct Quantile {
    double quantile;
    double error;
  };

  using Quantiles = vector<Quantile>;

  using trivially_relocatable = is_trivially_relocatable<Quantiles>::type;

  Quantiles quantiles;
  milliseconds max_age = seconds{60};
  int age_buckets = 5;
};

}  // namespace cct
