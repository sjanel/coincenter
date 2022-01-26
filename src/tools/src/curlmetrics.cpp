#include "curlmetrics.hpp"

namespace cct {
namespace {

MetricKeyPerRequestType DefineTypes(MetricKey &requestCountKey) {
  MetricKeyPerRequestType ret;
  for (HttpRequestType requestType : kAllHttpRequestsTypes) {
    requestCountKey.set("type", ToString(requestType));
    ret.insert_or_assign(requestType, requestCountKey);
  }
  return ret;
}

MetricKeyPerRequestType CreateNbRequestsMetricKeys() {
  MetricKey requestCountKey = CreateMetricKey("http_request_count", "Counter of http requests");
  return DefineTypes(requestCountKey);
}

MetricKeyPerRequestType CreateRequestDurationMetricKeys() {
  MetricKey requestCountKey = CreateMetricKey("http_request_duration_ms", "Duration of http requests in milliseconds");
  return DefineTypes(requestCountKey);
}

}  // namespace

const MetricKeyPerRequestType CurlMetrics::kNbRequestsKeys = CreateNbRequestsMetricKeys();
const MetricKeyPerRequestType CurlMetrics::kRequestDurationKeys = CreateRequestDurationMetricKeys();
}  // namespace cct