#pragma once

#include <chrono>
#include <string_view>
#include <utility>

#include "cct_string.hpp"
#include "curloptions.hpp"
#include "runmodes.hpp"
#include "timehelpers.hpp"

namespace cct {

class AbstractMetricGateway;

/// RAII class safely managing a CURL handle.
///
/// Aim of this class is to simplify curl library complexity usage, and abstracts it from client
///
/// Note that this implementation is not thread-safe. It is recommended to embed an instance of
/// CurlHandle for faster similar queries.
class CurlHandle {
 public:
  /// Constructs a default CurlHandle without any min duration between queries nor support for metric collection.
  CurlHandle() : CurlHandle(nullptr, Clock::duration::zero(), settings::RunMode::kProd) {}

  /// Constructs a new CurlHandle.
  /// @param minDurationBetweenQueries delay query 'n + 1' in case query 'n' was too close
  /// @param pMetricGateway optional pointer to metrics gateway. If not null, metrics will be exported.
  CurlHandle(AbstractMetricGateway *pMetricGateway, Clock::duration minDurationBetweenQueries,
             settings::RunMode runMode);

  CurlHandle(const CurlHandle &) = delete;
  CurlHandle &operator=(const CurlHandle &) = delete;

  CurlHandle(CurlHandle &&o) noexcept;
  CurlHandle &operator=(CurlHandle &&o) noexcept;

  ~CurlHandle();

  string urlEncode(std::string_view url);

  string query(std::string_view url, const CurlOptions &opts);

  Clock::duration minDurationBetweenQueries() const { return _minDurationBetweenQueries; }

  // CurlHandle is trivially relocatable
  using trivially_relocatable = std::true_type;

 private:
  void checkHandleOrInit();
  void setUpProxy(const CurlOptions::ProxySettings &proxy);

  // void pointer instead of CURL to avoid having to forward declare (we don't know about the underlying definition)
  // and to avoid clients to pull unnecessary curl dependencies by just including the header
  void *_handle;
  AbstractMetricGateway *_pMetricGateway;
  Clock::duration _minDurationBetweenQueries;
  TimePoint _lastQueryTime;
};

struct CurlInitRAII {
  CurlInitRAII();

  CurlInitRAII(const CurlInitRAII &) = delete;
  CurlInitRAII &operator=(const CurlInitRAII &) = delete;

  CurlInitRAII(CurlInitRAII &&) = delete;
  CurlInitRAII &operator=(CurlInitRAII &&) = delete;

  ~CurlInitRAII();
};
}  // namespace cct