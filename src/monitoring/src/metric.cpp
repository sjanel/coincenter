#include "metric.hpp"

#include "cct_string.hpp"

namespace cct {
MetricKey CreateMetricKey(std::string_view name, std::string_view help) {
  string s("metric_name=");
  s.reserve(s.size() + name.size() + help.size() + 13U);
  s.append(name);
  s.append(",metric_help=");
  s.append(help);
  return MetricKey(std::move(s));
}
}  // namespace cct