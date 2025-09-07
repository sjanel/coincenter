#pragma once

#include <map>
#include <string_view>
#include <type_traits>

#include "besturlpicker.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "permanentcurloptions.hpp"
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
  CurlHandle() noexcept = default;

  /// Constructs a new CurlHandle.
  /// @param bestURLPicker object managing which URL to pick at each query based on response time stats
  /// @param pMetricGateway if not null, queries will export some metrics
  /// @param permanentCurlOptions curl options applied once and for all requests of this CurlHandle
  /// @param runMode run mode
  explicit CurlHandle(BestURLPicker bestURLPicker, AbstractMetricGateway *pMetricGateway = nullptr,
                      const PermanentCurlOptions &permanentCurlOptions = PermanentCurlOptions(),
                      settings::RunMode runMode = settings::RunMode::kProd);

  CurlHandle(const CurlHandle &) = delete;
  CurlHandle &operator=(const CurlHandle &) = delete;

  CurlHandle(CurlHandle &&rhs) noexcept;
  CurlHandle &operator=(CurlHandle &&rhs) noexcept;

  ~CurlHandle();

  /// Launch a query on the given endpoint, it should start with a '/' and not contain the base URLs given at
  /// creation of this object.
  /// Response is returned as a std::string_view to a memory hold in cache by this CurlHandle.
  /// The pointed memory is valid until a next call to 'query'.
  std::string_view query(std::string_view endpoint, const CurlOptions &opts);

  [[nodiscard]] std::string_view getNextBaseUrl() const { return _bestURLPicker.getNextBaseURL(); }

  [[nodiscard]] Duration minDurationBetweenQueries() const { return _minDurationBetweenQueries; }

  /// Instead of actually performing real calls, instructs this CurlHandle to
  /// return hardcoded responses (in values of given map) based on query endpoints with appended options (in key of
  /// given map).
  /// This should be used only for tests purposes, as the search for the matching query is of linear
  /// complexity in a flat key value string.
  void setOverridenQueryResponses(const std::map<string, string> &queryResponsesMap);

  void swap(CurlHandle &rhs) noexcept;

  /// CurlHandle is not trivially relocatable
  /// curl handle stores the address of the _queryData string (CURLOPT_WRITEDATA)
  using trivially_relocatable = std::false_type;

 private:
  void setUpProxy(const char *proxyUrl, bool reset);
  void setWriteData();

  // void pointer instead of CURL to avoid having to forward declare (we don't know about the underlying definition)
  // and to avoid clients to pull unnecessary curl dependencies by just including the header
  void *_handle = nullptr;
  AbstractMetricGateway *_pMetricGateway = nullptr;  // non-owning pointer
  Duration _minDurationBetweenQueries{};
  TimePoint _lastQueryTime;
  BestURLPicker _bestURLPicker;
  string _queryData;
  LogLevel _requestCallLogLevel = LogLevel::off;
  LogLevel _requestAnswerLogLevel = LogLevel::off;
  int _nbMaxRetries = PermanentCurlOptions::kDefaultNbMaxRetries;
  PermanentCurlOptions::TooManyErrorsPolicy _tooManyErrorsPolicy = PermanentCurlOptions::TooManyErrorsPolicy::kThrow;
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