#include "curlmetrics.hpp"

#include "httprequesttype.hpp"
#include "metric.hpp"

namespace cct {
namespace {

MetricKeyPerRequestType DefineTypes(MetricKey &requestCountKey) {
  MetricKeyPerRequestType ret;
  for (HttpRequestType requestType : kHttpRequestTypes) {
    requestCountKey.set("type", IntegralToString(requestType));
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

MetricKeyPerRequestType CreateNbRequestErrorsMetricKeys() {
  MetricKey requestCountKey = CreateMetricKey("http_request_error_count", "Counter of http request errors");
  return DefineTypes(requestCountKey);
}

}  // namespace

const MetricKeyPerRequestType CurlMetrics::kNbRequestsKeys = CreateNbRequestsMetricKeys();
const MetricKeyPerRequestType CurlMetrics::kRequestDurationKeys = CreateRequestDurationMetricKeys();
const MetricKeyPerRequestType CurlMetrics::kNbRequestErrorKeys = CreateNbRequestErrorsMetricKeys();
}  // namespace cct