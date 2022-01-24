#include "prometheusmetricgateway.hpp"

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/summary.h>

#include <cassert>
#include <string>

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "timehelpers.hpp"

#ifdef _MSC_VER
#include <Winsock2.h>
#else
#include <unistd.h>
#endif

namespace cct {

namespace {
constexpr int kHTTPSuccessReturnCode = 200;

// Constants to control frequency of flushes to Prometheus instance
constexpr Duration kPrometheusAutoFlushPeriod = std::chrono::minutes(3);
constexpr int kCheckFlushCounter = 20;

std::string GetHostName() {
  char hostname[1024];
  if (::gethostname(hostname, sizeof(hostname))) {
    hostname[0] = '\0';
  }
  return hostname;
}

}  // namespace

PrometheusMetricGateway::PrometheusMetricGateway(const MonitoringInfo& monitoringInfo)
    : AbstractMetricGateway(monitoringInfo),
      _gateway(std::string(monitoringInfo.address()), std::to_string(monitoringInfo.port()),
               std::string(monitoringInfo.jobName()), prometheus::Gateway::GetInstanceLabel(GetHostName()),
               std::string(monitoringInfo.username()), std::string(monitoringInfo.password())),
      _registry(std::make_shared<Registry>()),
      _lastFlushedTime(Clock::now()),
      _checkFlushCounter(0) {
  // create a push gateway
  _gateway.RegisterCollectable(_registry);
}

PrometheusMetricGateway::~PrometheusMetricGateway() {
  // We should not throw in a destructor - catch any exception and do nothing, not even a log (it could throw)
  try {
    flush();
  } catch (...) {
  }
}

namespace {
using ExtractedDataFromMetricKey = std::tuple<std::map<std::string, std::string>, std::string, std::string>;

inline ExtractedDataFromMetricKey ExtractData(const MetricKey& key) {
  ExtractedDataFromMetricKey ret;
  std::string_view metricNameSV, metricHelpSV;
  for (const auto& [k, v] : key) {
    if (k == kMetricNameKey) {
      metricNameSV = v;
    } else if (k == kMetricHelpKey) {
      metricHelpSV = v;
    } else {
      std::get<0>(ret).insert_or_assign(std::string(k), std::string(v));
    }
  }
  std::get<1>(ret) = std::string(metricNameSV);
  std::get<2>(ret) = std::string(metricHelpSV);
  return ret;
}

}  // namespace

void PrometheusMetricGateway::add(MetricType type, MetricOperation op, const MetricKey& key, double v) {
  assert(key.contains(kMetricNameKey) && key.contains(kMetricHelpKey));
  std::lock_guard<std::mutex> guard(_familiesMapMutex);
  auto foundIt = _familiesMap.find(key);
  switch (type) {
    case MetricType::kCounter: {
      prometheus::Counter* counterPtr;
      if (foundIt == _familiesMap.end()) {
        auto data = ExtractData(key);
        auto& builder = prometheus::BuildCounter().Name(std::get<1>(data)).Help(std::get<2>(data)).Register(*_registry);
        counterPtr =
            std::addressof(reinterpret_cast<prometheus::Family<prometheus::Counter>&>(builder).Add(std::get<0>(data)));
        _familiesMap.insert_or_assign(key, counterPtr);
      } else {
        counterPtr = reinterpret_cast<prometheus::Counter*>(foundIt->second);
      }
      switch (op) {
        case MetricOperation::kIncrement:
          if (v == 0) {
            counterPtr->Increment();
          } else {
            counterPtr->Increment(v);
          }
          break;
        default:
          throw exception("Unsupported metric operation");
      }
      break;
    }
    case MetricType::kGauge: {
      prometheus::Gauge* gaugePtr;
      if (foundIt == _familiesMap.end()) {
        auto data = ExtractData(key);
        auto& builder = prometheus::BuildGauge().Name(std::get<1>(data)).Help(std::get<2>(data)).Register(*_registry);
        gaugePtr =
            std::addressof(reinterpret_cast<prometheus::Family<prometheus::Gauge>&>(builder).Add(std::get<0>(data)));
        _familiesMap.insert_or_assign(key, gaugePtr);
      } else {
        gaugePtr = reinterpret_cast<prometheus::Gauge*>(foundIt->second);
      }
      switch (op) {
        case MetricOperation::kIncrement:
          if (v == 0) {
            gaugePtr->Increment();
          } else {
            gaugePtr->Increment(v);
          }
          break;
        case MetricOperation::kDecrement:
          if (v == 0) {
            gaugePtr->Decrement();
          } else {
            gaugePtr->Decrement(v);
          }
          break;
        case MetricOperation::kSet:
          gaugePtr->Set(v);
          break;
        case MetricOperation::kSetCurrentTime:
          gaugePtr->SetToCurrentTime();
          break;
        default:
          throw exception("Unsupported metric operation");
      }

      break;
    }
    case MetricType::kHistogram:
      if (foundIt == _familiesMap.end()) {
        log::error("You should create histogram first before adding any value in it");
      } else {
        prometheus::Histogram* histogramPtr = reinterpret_cast<prometheus::Histogram*>(foundIt->second);
        switch (op) {
          case MetricOperation::kObserve:
            histogramPtr->Observe(v);
            break;
          default:
            throw exception("Unsupported metric operation");
        }
      }
      break;

    case MetricType::kSummary:
      if (foundIt == _familiesMap.end()) {
        log::error("You should create summary first before adding any value in it");
      } else {
        prometheus::Summary* summaryPtr = reinterpret_cast<prometheus::Summary*>(foundIt->second);
        switch (op) {
          case MetricOperation::kObserve:
            summaryPtr->Observe(v);
            break;
          default:
            throw exception("Unsupported metric operation");
        }
      }
  }
  checkFlush();
}

void PrometheusMetricGateway::createHistogram(const MetricKey& key, BucketBoundaries buckets) {
  std::lock_guard<std::mutex> guard(_familiesMapMutex);
  if (_familiesMap.find(key) == _familiesMap.end()) {
    auto data = ExtractData(key);
    auto& builder = prometheus::BuildHistogram().Name(std::get<1>(data)).Help(std::get<2>(data)).Register(*_registry);
    prometheus::Histogram::BucketBoundaries prometheusBuckets(buckets.begin(), buckets.end());
    prometheus::Histogram* histogramPtr =
        std::addressof(reinterpret_cast<prometheus::Family<prometheus::Histogram>&>(builder).Add(
            std::get<0>(data), std::move(prometheusBuckets)));
    _familiesMap.insert_or_assign(key, histogramPtr);
  } else {
    log::warn("Prometheus histogram already created");
  }
}

void PrometheusMetricGateway::createSummary(const MetricKey& key, const MetricSummaryInfo& metricSummaryInfo) {
  std::lock_guard<std::mutex> guard(_familiesMapMutex);
  if (_familiesMap.find(key) == _familiesMap.end()) {
    auto data = ExtractData(key);
    auto& builder = prometheus::BuildSummary().Name(std::get<1>(data)).Help(std::get<2>(data)).Register(*_registry);
    prometheus::Summary::Quantiles prometheusQuantiles;
    prometheusQuantiles.reserve(metricSummaryInfo.quantiles.size());
    for (const MetricSummaryInfo::Quantile& quantile : metricSummaryInfo.quantiles) {
      prometheusQuantiles.emplace_back(quantile.quantile, quantile.error);
    }
    prometheus::Summary* summaryPtr =
        std::addressof(reinterpret_cast<prometheus::Family<prometheus::Summary>&>(builder).Add(
            std::get<0>(data), std::move(prometheusQuantiles), metricSummaryInfo.max_age,
            metricSummaryInfo.age_buckets));
    _familiesMap.insert_or_assign(key, summaryPtr);
  } else {
    log::warn("Prometheus summary already created");
  }
}

void PrometheusMetricGateway::flush() {
  auto nowTime = Clock::now();
  int returnCode = _gateway.Push();
  if (returnCode == kHTTPSuccessReturnCode) {
    log::info("Flushed data to Prometheus in {} ms",
              std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - nowTime).count());
  } else {
    log::error("Unable to push metrics to Prometheus instance - Bad return code {}", returnCode);
  }
}

void PrometheusMetricGateway::checkFlush() {
  if ((++_checkFlushCounter % kCheckFlushCounter) == 0) {
    auto nowTime = Clock::now();
    if (_lastFlushedTime + kPrometheusAutoFlushPeriod < nowTime) {
      flush();
      _lastFlushedTime = Clock::now();
    }
  }
}

}  // namespace cct
