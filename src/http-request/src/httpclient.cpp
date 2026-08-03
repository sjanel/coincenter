#include "httpclient.hpp"

#include <aeronet/client-request.hpp>
#include <aeronet/http-client-config.hpp>
#include <aeronet/http-client-error.hpp>
#include <aeronet/http-client.hpp>
#include <aeronet/http-method.hpp>
#include <aeronet/http-response.hpp>
#include <aeronet/version.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "abstractmetricgateway.hpp"
#include "besturlpicker.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "durationstring.hpp"
#include "flatkeyvaluestring.hpp"
#include "httpmetrics.hpp"
#include "httppostdata.hpp"
#include "httprequestoptions.hpp"
#include "httprequesttype.hpp"
#include "metric.hpp"
#include "permanentrequestoptions.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"
#include "unreachable.hpp"

namespace cct {

namespace {

/// According to RFC3986 (https://www.rfc-editor.org/rfc/rfc3986#section-2)
/// '"' cannot be used in a URI (not percent encoded), so it's a fine delimiter for our FlatQueryResponse map
using FlatQueryResponseMap = FlatKeyValueString<'\0', '"'>;

aeronet::http::Method ToAeronetMethod(HttpRequestType requestType) {
  switch (requestType) {
    case HttpRequestType::kGet:
      return aeronet::http::Method::GET;
    case HttpRequestType::kPost:
      return aeronet::http::Method::POST;
    case HttpRequestType::kDelete:
      return aeronet::http::Method::DELETE;
    default:
      unreachable();
  }
}

bool IsContentTypeHeader(std::string_view key) {
  static constexpr std::string_view kContentType = "content-type";
  return std::ranges::equal(key, kContentType, [](char lhs, char rhs) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(lhs))) == rhs;
  });
}

}  // namespace

string GetHttpClientVersionInfo() {
  // Full multi-line breakdown: aeronet + glaze + TLS + logging + compression versions.
  return string(aeronet::fullVersionWithRuntime());
}

HttpClient::HttpClient() noexcept = default;

HttpClient::HttpClient(BestURLPicker bestURLPicker, AbstractMetricGateway *pMetricGateway,
                       const PermanentRequestOptions &permanentRequestOptions, settings::RunMode runMode)
    : _pMetricGateway(pMetricGateway),
      _minDurationBetweenQueries(permanentRequestOptions.minDurationBetweenQueries()),
      _bestURLPicker(std::move(bestURLPicker)),
      _requestCallLogLevel(permanentRequestOptions.requestCallLogLevel()),
      _requestAnswerLogLevel(permanentRequestOptions.requestAnswerLogLevel()),
      _nbMaxRetries(permanentRequestOptions.nbMaxRetries()),
      _tooManyErrorsPolicy(permanentRequestOptions.tooManyErrorsPolicy()) {
  if (settings::AreQueryResponsesOverriden(runMode)) {
    // Query response override mode: no real HTTP client is created, responses are served from a local map.
    return;
  }

  aeronet::HttpClientConfig config;

  const string &userAgent = permanentRequestOptions.getUserAgent();
  if (userAgent.empty()) {
    string defaultUserAgent = "coincenter ";
    defaultUserAgent.append(CCT_VERSION);
    defaultUserAgent.append(", aeronet ");
    defaultUserAgent.append(aeronet::version());
    config.withUserAgent(defaultUserAgent);
  } else {
    config.withUserAgent(userAgent);
  }

  // aeronet transparently advertises (Accept-Encoding) and decodes the compression codecs it was
  // compiled with (gzip / deflate / zstd / brotli). An explicit accepted encoding, when provided,
  // overrides that auto-advertising.
  const string &acceptedEncoding = permanentRequestOptions.getAcceptedEncoding();
  if (!acceptedEncoding.empty()) {
    config.withDefaultAcceptEncoding(acceptedEncoding);
  }

  config.followRedirects = permanentRequestOptions.followLocation();

  if (permanentRequestOptions.timeout() != Duration{}) {
    config.requestTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(permanentRequestOptions.timeout());
  }

  _client = new aeronet::HttpClient(std::move(config));

  log::debug("Initialize HttpClient for {} with {} as minimum duration between queries", _bestURLPicker.getNextBaseURL(),
             DurationToString(_minDurationBetweenQueries));

  if (settings::IsProxyRequested(runMode)) {
    // aeronet's HttpClient does not (yet) support forwarding through an HTTP proxy.
    log::warn("Proxy run mode requested but the aeronet HTTP client has no forward-proxy support yet; ignoring proxy");
  }
}

std::string_view HttpClient::query(std::string_view endpoint, const HttpRequestOptions &opts) {
  const HttpPostData &postData = opts.postData();
  const bool queryResponseOverrideMode = !_client;
  const bool appendParametersInQueryStr =
      !postData.empty() && (opts.requestType() != HttpRequestType::kPost || queryResponseOverrideMode);

  const auto baseUrlPos = _bestURLPicker.nextBaseURLPos();
  const std::string_view baseUrl = _bestURLPicker.getBaseURL(baseUrlPos);
  const std::string_view postDataStr = postData.str();

  string modifiedURL(baseUrl.size() + endpoint.size() + (appendParametersInQueryStr ? (1U + postDataStr.size()) : 0U),
                     '?');

  auto modifiedUrlOutIt = std::ranges::copy(baseUrl, modifiedURL.begin()).out;
  modifiedUrlOutIt = std::ranges::copy(endpoint, modifiedUrlOutIt).out;
  string jsonStr;  // Declared here as its scope should be valid until the actual query call
  std::string_view optsStr;
  if (appendParametersInQueryStr) {
    modifiedUrlOutIt = std::ranges::copy(postDataStr, modifiedUrlOutIt + 1).out;
  } else if (opts.isPostDataInJsonFormat() && !postData.empty()) {
    jsonStr = postData.toJsonStr();
    optsStr = jsonStr;
  } else {
    optsStr = postData.str();
  }

  if (queryResponseOverrideMode) {
    // Query response override mode
    const std::string_view path(modifiedURL.begin() + baseUrl.size(), modifiedURL.end());
    const std::string_view response = FlatQueryResponseMap::Get(_queryData, path);
    if (response.empty()) {
      throw exception("No response for path '{}'", path);
    }
    return response;
  }

  // Build the request. Content-Type must be routed through body() (aeronet rejects it as a raw header).
  aeronet::ClientRequest req(ToAeronetMethod(opts.requestType()), modifiedURL);
  std::string_view contentType;
  for (const auto &header : opts.httpHeaders()) {
    if (IsContentTypeHeader(header.key())) {
      contentType = header.val();
    } else {
      req.headerAddLine(header.key(), header.val());
    }
  }

  if (opts.requestType() == HttpRequestType::kPost && (!optsStr.empty() || !contentType.empty())) {
    if (contentType.empty()) {
      contentType = opts.isPostDataInJsonFormat() ? "application/json" : "application/x-www-form-urlencoded";
    }
    req.body(optsStr, contentType);
  }

  if (_minDurationBetweenQueries != Duration::zero()) {
    // Check last request time
    const auto nowTime = Clock::now();
    if (nowTime < _lastQueryTime + _minDurationBetweenQueries) {
      // We should sleep a bit before performing query
      const Duration sleepingTime = _minDurationBetweenQueries - (nowTime - _lastQueryTime);
      log::trace("Wait {} before performing query", DurationToString(sleepingTime));
      std::this_thread::sleep_for(sleepingTime);
      _lastQueryTime = nowTime + sleepingTime;
    } else {
      // Query can be performed immediately
      _lastQueryTime = nowTime;
    }
  }

  auto nbRequestsDone = _bestURLPicker.nbRequestsDone();
  static constexpr auto kLogRequestsThreshold = 100;

  if (opts.requestType() != HttpRequestType::kGet || nbRequestsDone % kLogRequestsThreshold == 0) {
    const auto httpRequestStr = HttpRequestTypeToString(opts.requestType());
    log::log(static_cast<log::level::level_enum>(_requestCallLogLevel), "{} {}{}{}", httpRequestStr, modifiedURL,
             optsStr.empty() ? "" : "?", optsStr);
    if (nbRequestsDone == 0 && opts.requestType() == HttpRequestType::kGet) {
      log::log(static_cast<log::level::level_enum>(_requestCallLogLevel), "Will only log {} requests every {} calls",
               httpRequestStr, kLogRequestsThreshold);
    }
  }

  // Actually make the query, with a fast retry mechanism (to avoid random technical errors)
  Duration sleepingTime = milliseconds(100);
  int retryPos = 0;
  bool success = false;
  std::string_view lastErrStr;

  _queryData.clear();

  do {
    if (retryPos != 0) {
      if (_pMetricGateway != nullptr) {
        _pMetricGateway->add(MetricType::kCounter, MetricOperation::kIncrement,
                             HttpMetrics::kNbRequestErrorKeys.find(opts.requestType())->second);
      }
      log::error("Got HTTP client error '{}' for {}, retry {}/{} after {}", lastErrStr, modifiedURL, retryPos,
                 _nbMaxRetries, DurationToString(sleepingTime));
      std::this_thread::sleep_for(sleepingTime);
      sleepingTime *= 2;
    }

    auto t1 = Clock::now();

    // Call
    auto result = _client->request(req);

    // Store stats
    const auto queryRTInMs = static_cast<uint32_t>(GetTimeFrom<milliseconds>(t1).count());
    _bestURLPicker.storeResponseTimePerBaseURL(baseUrlPos, queryRTInMs);

    if (_pMetricGateway != nullptr) {
      _pMetricGateway->add(MetricType::kCounter, MetricOperation::kIncrement,
                           HttpMetrics::kNbRequestsKeys.find(opts.requestType())->second);
      _pMetricGateway->add(MetricType::kHistogram, MetricOperation::kObserve,
                           HttpMetrics::kRequestDurationKeys.find(opts.requestType())->second,
                           static_cast<double>(queryRTInMs));
    }

    // Periodic memory release to avoid memory leak for a very large number of requests
    static constexpr int kReleaseMemoryRequestsFrequency = 10000;
    if ((nbRequestsDone % kReleaseMemoryRequestsFrequency) == 0) {
      _queryData.shrink_to_fit();
    }

    ++nbRequestsDone;

    // A non-2xx HTTP status is NOT an error for aeronet (it is a normal response): exchanges parse the
    // status out of the JSON body, so we treat any received response as a success and only retry on a
    // transport-level failure (DNS / connect / timeout / TLS / malformed response...).
    if (result) {
      const std::string_view body = result->bodyInMemory();
      _queryData.assign(body.data(), body.size());
      success = true;
    } else {
      lastErrStr = aeronet::ErrcToStr(result.error());
    }
  } while (!success && ++retryPos <= _nbMaxRetries);

  if (!success) {
    switch (_tooManyErrorsPolicy) {
      case PermanentRequestOptions::TooManyErrorsPolicy::kReturnEmptyResponse:
        log::error("Too many errors from the HTTP client, return empty response");
        _queryData.clear();
        break;
      case PermanentRequestOptions::TooManyErrorsPolicy::kThrow:
        throw exception("Too many errors from the HTTP client, last ({})", lastErrStr);
      default:
        unreachable();
    }
  }

  // Avoid polluting the logs for large response which are more likely to be HTML
  const bool mayBeJsonResponse = _queryData.starts_with('{') || _queryData.starts_with('[');
  static constexpr std::size_t kMaxLenResponse = 1000;
  if (!mayBeJsonResponse && _queryData.size() > kMaxLenResponse) {
    const std::string_view outPrinted(_queryData.begin(),
                                      _queryData.begin() + std::min(_queryData.size(), kMaxLenResponse));
    log::log(static_cast<log::level::level_enum>(_requestCallLogLevel), "Truncated non JSON response {}...", outPrinted);
  } else {
    log::log(static_cast<log::level::level_enum>(_requestAnswerLogLevel), "Full{}JSON response {}",
             mayBeJsonResponse ? " " : " non ", _queryData);
  }

  return _queryData;
}

void HttpClient::setOverridenQueryResponses(const std::map<string, string> &queryResponsesMap) {
  if (_client) {
    throw exception(
        "HttpClient should be created in Query response override mode in order to override its next response");
  }
  FlatQueryResponseMap flatQueryResponses;
  for (const auto &[query, response] : queryResponsesMap) {
    flatQueryResponses.emplace_back(query, response);
  }
  _queryData = string(flatQueryResponses.str());
}

void HttpClient::swap(HttpClient &rhs) noexcept {
  using std::swap;

  swap(_client, rhs._client);
  swap(_pMetricGateway, rhs._pMetricGateway);
  swap(_minDurationBetweenQueries, rhs._minDurationBetweenQueries);
  swap(_lastQueryTime, rhs._lastQueryTime);
  swap(_bestURLPicker, rhs._bestURLPicker);
  _queryData.swap(rhs._queryData);
  swap(_requestCallLogLevel, rhs._requestCallLogLevel);
  swap(_requestAnswerLogLevel, rhs._requestAnswerLogLevel);
  swap(_nbMaxRetries, rhs._nbMaxRetries);
  swap(_tooManyErrorsPolicy, rhs._tooManyErrorsPolicy);
}

HttpClient::HttpClient(HttpClient &&rhs) noexcept { swap(rhs); }

HttpClient &HttpClient::operator=(HttpClient &&rhs) noexcept {
  swap(rhs);
  return *this;
}

HttpClient::~HttpClient() { delete _client; }

}  // namespace cct
