#pragma once

#include <map>
#include <string_view>
#include <type_traits>

#include "besturlpicker.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "permanentrequestoptions.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace aeronet {
class HttpClient;
}

namespace cct {

class AbstractMetricGateway;
class HttpRequestOptions;

// Get a string with the runtime version information of the underlying HTTP client (aeronet) and its
// linked libraries (OpenSSL, compression codecs...).
string GetHttpClientVersionInfo();

// Sentinel "empty" base URL. Unlike the former libcurl handle (optimized around a single connected host),
// aeronet's HttpClient is not bound to one endpoint, so a single HttpClient can serve several distinct
// hosts. Construct it with kNoBaseUrl and pass the full, absolute URL to each query() call.
inline constexpr std::string_view kNoBaseUrl;

/// Wrapper around an aeronet HTTP client managing all the coincenter specificities on top of raw HTTP:
///  - selection of the best base URL among several ones based on response time statistics (BestURLPicker)
///  - export of request metrics to an optional metric gateway
///  - throttling (minimum duration between two queries)
///  - a fast retry mechanism on transient transport errors
///  - request / answer logging
///  - a query-response override mode used by unit tests (no external call is made)
///
/// Note that this implementation is not thread-safe (the underlying aeronet::HttpClient owns a single
/// event loop and connection pool). It is recommended to embed an instance of HttpClient for faster
/// similar queries.
class HttpClient {
 public:
  HttpClient() noexcept;

  /// Constructs a new HttpClient.
  /// @param bestURLPicker object managing which URL to pick at each query based on response time stats
  /// @param pMetricGateway if not null, queries will export some metrics
  /// @param permanentRequestOptions options applied once and for all requests of this HttpClient
  /// @param runMode run mode
  explicit HttpClient(BestURLPicker bestURLPicker, AbstractMetricGateway* pMetricGateway = nullptr,
                      const PermanentRequestOptions& permanentRequestOptions = PermanentRequestOptions(),
                      settings::RunMode runMode = settings::RunMode::kProd);

  HttpClient(const HttpClient&) = delete;
  HttpClient& operator=(const HttpClient&) = delete;

  HttpClient(HttpClient&& rhs) noexcept;
  HttpClient& operator=(HttpClient&& rhs) noexcept;

  ~HttpClient();

  /// Launch a query on the given endpoint, it should start with a '/' and not contain the base URLs given at
  /// creation of this object.
  /// Response is returned as a std::string_view to a memory hold in cache by this HttpClient.
  /// The pointed memory is valid until a next call to 'query'.
  std::string_view query(std::string_view endpoint, const HttpRequestOptions& opts);

  [[nodiscard]] std::string_view getNextBaseUrl() const { return _bestURLPicker.getNextBaseURL(); }

  [[nodiscard]] Duration minDurationBetweenQueries() const { return _minDurationBetweenQueries; }

  /// Instead of actually performing real calls, instructs this HttpClient to
  /// return hardcoded responses (in values of given map) based on query endpoints with appended options (in key of
  /// given map).
  /// This should be used only for tests purposes, as the search for the matching query is of linear
  /// complexity in a flat key value string.
  void setOverridenQueryResponses(const std::map<string, string>& queryResponsesMap);

  void swap(HttpClient& rhs) noexcept;

  /// HttpClient is not trivially relocatable: the underlying aeronet::HttpClient owns an event loop
  /// and reusable buffers referenced by internal state.
  using trivially_relocatable = std::false_type;

 private:
  // Owning raw pointer to the underlying aeronet HTTP client (deleted in the destructor). A raw pointer
  // behind a forward declaration is used - rather than a unique_ptr - so that clients including this
  // header (and the unit-test mocks that redefine this class' members) do not need the complete aeronet
  // type. A null pointer means the object runs in query-response override mode (unit tests).
  aeronet::HttpClient* _client = nullptr;
  AbstractMetricGateway* _pMetricGateway = nullptr;  // non-owning pointer
  Duration _minDurationBetweenQueries{};
  TimePoint _lastQueryTime;
  BestURLPicker _bestURLPicker;
  string _queryData;
  LogLevel _requestCallLogLevel = LogLevel::off;
  LogLevel _requestAnswerLogLevel = LogLevel::off;
  int _nbMaxRetries = PermanentRequestOptions::kDefaultNbMaxRetries;
  PermanentRequestOptions::TooManyErrorsPolicy _tooManyErrorsPolicy =
      PermanentRequestOptions::TooManyErrorsPolicy::kThrow;
};

}  // namespace cct
