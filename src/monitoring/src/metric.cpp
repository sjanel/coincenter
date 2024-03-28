#include "metric.hpp"

#include <string_view>

namespace cct {
MetricKey CreateMetricKey(std::string_view name, std::string_view help) {
  MetricKey key;

  key.emplace_back(kMetricNameKey, name);
  key.emplace_back(kMetricHelpKey, help);

  return key;
}
}  // namespace cct