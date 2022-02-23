#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include "cct_fixedcapacityvector.hpp"
#include "cct_string.hpp"
#include "curloptions.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace cct {

class AbstractMetricGateway;

/// RAII class safely managing a CURL handle.
///
/// Aim of this class is to simplify curl library complexity usage, and abstracts it from client
///
/// Note that this implementation is not thread-safe. It is recommended to embed an instance of
/// CurlHandle for faster similar queries.
class CurlHandle {
 private:
  static constexpr int kNbMaxBaseUrl = 4;

 public:
  /// Constructs a new CurlHandle with only one possible Base URL without any min duration between queries nor support
  /// for metric collection.
  /// Warning: given base URL should come from static storage
  explicit CurlHandle(const std::string_view &singleBaseUrl)
      : CurlHandle(std::addressof(singleBaseUrl), 1, nullptr, Duration::zero(), settings::RunMode::kProd) {}

  CurlHandle(const string &) = delete;
  CurlHandle(const char *) = delete;

  /// Constructs a new CurlHandle with only one possible Base URL.
  /// Warning: given base URL should come from static storage
  /// @param minDurationBetweenQueries delay query 'n + 1' in case query 'n' was too close
  /// @param pMetricGateway optional pointer to metrics gateway. If not null, metrics will be exported.
  CurlHandle(const std::string_view &singleBaseUrl, AbstractMetricGateway *pMetricGateway,
             Duration minDurationBetweenQueries, settings::RunMode runMode)
      : CurlHandle(std::addressof(singleBaseUrl), 1, pMetricGateway, minDurationBetweenQueries, runMode) {}

  CurlHandle(const string &, AbstractMetricGateway *, Duration, settings::RunMode) = delete;
  CurlHandle(const char *, AbstractMetricGateway *, Duration, settings::RunMode) = delete;

  /// Constructs a new CurlHandle without any min duration between queries nor support
  /// for metric collection.
  /// Warning: given base URL should come from static storage
  /// @param minDurationBetweenQueries delay query 'n + 1' in case query 'n' was too close
  /// @param pMetricGateway optional pointer to metrics gateway. If not null, metrics will be exported.
  template <unsigned N, std::enable_if_t<(N > 0) && N <= kNbMaxBaseUrl, bool> = true>
  explicit CurlHandle(const std::string_view (&aBaseUrl)[N])
      : CurlHandle(aBaseUrl, N, nullptr, Duration::zero(), settings::RunMode::kProd) {}

  CurlHandle(const string[]) = delete;
  CurlHandle(const char *[]) = delete;

  /// Constructs a new CurlHandle.
  /// Warning: given base URL should come from static storage
  /// @param minDurationBetweenQueries delay query 'n + 1' in case query 'n' was too close
  /// @param pMetricGateway optional pointer to metrics gateway. If not null, metrics will be exported.
  template <unsigned N, std::enable_if_t<(N > 0) && N <= kNbMaxBaseUrl, bool> = true>
  CurlHandle(const std::string_view (&aBaseUrl)[N], AbstractMetricGateway *pMetricGateway,
             Duration minDurationBetweenQueries, settings::RunMode runMode)
      : CurlHandle(aBaseUrl, N, pMetricGateway, minDurationBetweenQueries, runMode) {}

  CurlHandle(const string[], AbstractMetricGateway *, Duration, settings::RunMode) = delete;
  CurlHandle(const char *[], AbstractMetricGateway *, Duration, settings::RunMode) = delete;

  CurlHandle(const CurlHandle &) = delete;
  CurlHandle &operator=(const CurlHandle &) = delete;

  CurlHandle(CurlHandle &&) = delete;
  CurlHandle &operator=(CurlHandle &&) = delete;

  ~CurlHandle();

  /// URL Encode using cURL encode algorithm on the given string
  string urlEncode(std::string_view data) const;

  /// Launch a query on the given endpoint, it should start with a '/' and not contain the base URLs given at
  /// creation of this object.
  string query(std::string_view endpoint, const CurlOptions &opts);

  // Return the next base url that will be used by the next query
  std::string_view getNextBaseUrl() const { return _pBaseUrls[pickBestBaseUrlPos()]; }

  Duration minDurationBetweenQueries() const { return _minDurationBetweenQueries; }

  // CurlHandle is trivially relocatable
  using trivially_relocatable = std::true_type;

 private:
  struct ResponseTimeStats {
    uint32_t nbRequestsDone;
    uint16_t avgResponseTime;
    uint16_t avgDeviation;
  };

  using ResponseTimeStatsPerBaseUrl = FixedCapacityVector<ResponseTimeStats, kNbMaxBaseUrl>;

  CurlHandle(const std::string_view *pBaseUrlStartPtr, int8_t nbBaseUrl, AbstractMetricGateway *pMetricGateway,
             Duration minDurationBetweenQueries, settings::RunMode runMode);

  void setUpProxy(const char *proxyUrl, bool reset);

  int8_t pickBestBaseUrlPos() const;
  void storeResponseTimePerBaseUrl(int8_t baseUrlPos, uint32_t responseTimeInMs);

  int8_t nbBaseUrl() const { return static_cast<int8_t>(_responseTimeStatsPerBaseUrl.size()); }

  // void pointer instead of CURL to avoid having to forward declare (we don't know about the underlying definition)
  // and to avoid clients to pull unnecessary curl dependencies by just including the header
  void *_handle;
  AbstractMetricGateway *_pMetricGateway;  // non-owning pointer
  const std::string_view *_pBaseUrls;
  Duration _minDurationBetweenQueries;
  TimePoint _lastQueryTime{};
  ResponseTimeStatsPerBaseUrl _responseTimeStatsPerBaseUrl;
};

struct CurlInitRAII {
  [[nodiscard]] CurlInitRAII();

  CurlInitRAII(const CurlInitRAII &) = delete;
  CurlInitRAII &operator=(const CurlInitRAII &) = delete;

  CurlInitRAII(CurlInitRAII &&) = delete;
  CurlInitRAII &operator=(CurlInitRAII &&) = delete;

  ~CurlInitRAII();
};
}  // namespace cct