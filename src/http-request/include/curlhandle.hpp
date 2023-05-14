#pragma once

#include <string_view>
#include <type_traits>
#include <utility>

#include "besturlpicker.hpp"
#include "cct_string.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace cct {

class AbstractMetricGateway;
class CurlOptions;

// Get a string returning runtime curl version information.
string GetCurlVersionInfo();

/// RAII class safely managing a CURL handle.
///
/// Aim of this class is to simplify curl library complexity usage, and abstracts it from client
///
/// Note that this implementation is not thread-safe. It is recommended to embed an instance of
/// CurlHandle for faster similar queries.
class CurlHandle {
 public:
  /// Constructs a new CurlHandle.
  /// @param bestURLPicker object managing which URL to pick at each query based on response time stats
  /// @param pMetricGateway if not null, queries will export some metrics
  /// @param minDurationBetweenQueries delay query 'n + 1' in case query 'n' was too close
  /// @param runMode run mode
  explicit CurlHandle(const BestURLPicker &bestURLPicker, AbstractMetricGateway *pMetricGateway = nullptr,
                      Duration minDurationBetweenQueries = Duration::zero(),
                      settings::RunMode runMode = settings::RunMode::kProd);

  // Move operations are deleted but could be implemented if needed. It's just to avoid useless code.
  CurlHandle(const CurlHandle &) = delete;
  CurlHandle &operator=(const CurlHandle &) = delete;

  CurlHandle(CurlHandle &&) = delete;
  CurlHandle &operator=(CurlHandle &&) = delete;

  ~CurlHandle();

  /// Launch a query on the given endpoint, it should start with a '/' and not contain the base URLs given at
  /// creation of this object.
  string query(std::string_view endpoint, const CurlOptions &opts);

  std::string_view getNextBaseUrl() const { return _bestUrlPicker.getNextBaseURL(); }

  Duration minDurationBetweenQueries() const { return _minDurationBetweenQueries; }

  // CurlHandle is trivially relocatable
  using trivially_relocatable = std::true_type;

 private:
  void setUpProxy(const char *proxyUrl, bool reset);

  // void pointer instead of CURL to avoid having to forward declare (we don't know about the underlying definition)
  // and to avoid clients to pull unnecessary curl dependencies by just including the header
  void *_handle;
  AbstractMetricGateway *_pMetricGateway;  // non-owning pointer
  Duration _minDurationBetweenQueries;
  TimePoint _lastQueryTime{};
  BestURLPicker _bestUrlPicker;
};

// Simple RAII class managing global init and clean up of Curl library.
// It's in the same file as CurlHandle so that only one source file has a dependency on curl sources.
struct CurlInitRAII {
  [[nodiscard]] CurlInitRAII();

  CurlInitRAII(const CurlInitRAII &) = delete;
  CurlInitRAII &operator=(const CurlInitRAII &) = delete;

  CurlInitRAII(CurlInitRAII &&) = delete;
  CurlInitRAII &operator=(CurlInitRAII &&) = delete;

  ~CurlInitRAII();
};
}  // namespace cct