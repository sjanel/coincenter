#include "metric.hpp"

#include "cct_string.hpp"

namespace cct {
MetricKey CreateMetricKey(std::string_view name, std::string_view help) {
  MetricKey key;
  key.append(kMetricNameKey, name);
  key.append(kMetricHelpKey, help);
  return key;
}
}  // namespace cct