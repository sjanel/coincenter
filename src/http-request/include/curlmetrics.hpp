#pragma once

#include <map>

#include "httprequesttype.hpp"
#include "metric.hpp"

namespace cct {
using MetricKeyPerRequestType = std::map<HttpRequestType, MetricKey>;

struct CurlMetrics {
  static const MetricKeyPerRequestType kNbRequestsKeys;
  static const MetricKeyPerRequestType kRequestDurationKeys;
  static const MetricKeyPerRequestType kNbRequestErrorKeys;
};

}  // namespace cct