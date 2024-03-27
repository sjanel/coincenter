#include "metric.hpp"

#include <string_view>

namespace cct {
MetricKey CreateMetricKey(std::string_view name, std::string_view help) {
  MetricKey key;

  key.push_back(kMetricNameKey, name);
  key.push_back(kMetricHelpKey, help);

  return key;
}
}  // namespace cct