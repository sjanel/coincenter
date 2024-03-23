#include "curlhandle.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

#include "abstractmetricgateway.hpp"
#include "besturlpicker.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "curlmetrics.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "durationstring.hpp"
#include "flatkeyvaluestring.hpp"
#include "httprequesttype.hpp"
#include "metric.hpp"
#include "permanentcurloptions.hpp"
#include "proxy.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"
#include "unreachable.hpp"

namespace cct {

namespace {

/// According to RFC3986 (https://www.rfc-editor.org/rfc/rfc3986#section-2)
/// '"' cannot be used in a URI (not percent encoded), so it's a fine delimiter for our FlatQueryResponse map
using FlatQueryResponseMap = FlatKeyValueString<'\0', '"'>;

size_t CurlWriteCallback(const char *contents, size_t size, size_t nmemb, void *userp) {
  try {
    reinterpret_cast<string *>(userp)->append(contents, size * nmemb);
  } catch (const std::bad_alloc &e) {
    // Do not throw exceptions in a function passed to a C library
    // Returning 0 is a magic number that will cause CURL to raise an error
    log::error("Bad alloc caught in curl write call back action, returning 0: {}", e.what());
    return 0;
  }
  return size * nmemb;
}

template <class T>
void CurlSetLogIfError(CURL *curl, CURLoption curlOption, T value) {
  static_assert(std::is_integral_v<T> || std::is_pointer_v<T>);
  const CURLcode code = curl_easy_setopt(curl, curlOption, value);
  if (code != CURLE_OK) {
    if constexpr (std::is_integral_v<T> || std::is_same_v<T, const char *>) {
      log::error("Curl error {} setting option {} to {}", static_cast<int>(code), static_cast<int>(curlOption), value);
    } else {
      log::error("Curl error {} setting option {}", static_cast<int>(code), static_cast<int>(curlOption));
    }
  }
}
}  // namespace

string GetCurlVersionInfo() {
  const curl_version_info_data &curlVersionInfo = *curl_version_info(CURLVERSION_NOW);

  string curlVersionInfoStr("curl ");
  curlVersionInfoStr.append(curlVersionInfo.version);
  if (curlVersionInfo.ssl_version == nullptr) {
    throw exception("Invalid curl install - no ssl support");
  }
  curlVersionInfoStr.append(" ssl ").append(curlVersionInfo.ssl_version);
  if (curlVersionInfo.libz_version != nullptr) {
    curlVersionInfoStr.append(" libz ").append(curlVersionInfo.libz_version);
  } else {
    curlVersionInfoStr.append(" NO libz support");
  }
  return curlVersionInfoStr;
}

CurlHandle::CurlHandle(BestURLPicker bestURLPicker, AbstractMetricGateway *pMetricGateway,
                       const PermanentCurlOptions &permanentCurlOptions, settings::RunMode runMode)
    : _pMetricGateway(pMetricGateway),
      _minDurationBetweenQueries(permanentCurlOptions.minDurationBetweenQueries()),
      _bestURLPicker(std::move(bestURLPicker)),
      _requestCallLogLevel(permanentCurlOptions.requestCallLogLevel()),
      _requestAnswerLogLevel(permanentCurlOptions.requestAnswerLogLevel()),
      _nbMaxRetries(permanentCurlOptions.nbMaxRetries()),
      _tooManyErrorsPolicy(permanentCurlOptions.tooManyErrorsPolicy()) {
  if (!settings::AreQueryResponsesOverriden(runMode)) {
    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
      throw std::bad_alloc();
    }

    _handle = curl;

    const string &userAgent = permanentCurlOptions.getUserAgent();
    if (userAgent.empty()) {
      string defaultUserAgent = "coincenter ";
      defaultUserAgent.append(CCT_VERSION);
      defaultUserAgent.append(", ");
      defaultUserAgent.append(GetCurlVersionInfo());

      CurlSetLogIfError(curl, CURLOPT_USERAGENT, defaultUserAgent.data());
    } else {
      CurlSetLogIfError(curl, CURLOPT_USERAGENT, userAgent.data());
    }
    CurlSetLogIfError(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    CurlSetLogIfError(curl, CURLOPT_WRITEDATA, &_queryData);
    const string &acceptedEncoding = permanentCurlOptions.getAcceptedEncoding();
    if (!acceptedEncoding.empty()) {
      CurlSetLogIfError(curl, CURLOPT_ACCEPT_ENCODING, acceptedEncoding.data());
    }

    CurlSetLogIfError(curl, CURLOPT_FOLLOWLOCATION, permanentCurlOptions.followLocation() ? 1L : 0L);

#ifdef CCT_MSVC
    // https://stackoverflow.com/questions/37551409/configure-curl-to-use-default-system-cert-store-on-windows
    CurlSetLogIfError(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif

    log::debug("Initialize CurlHandle for {} with {} as minimum duration between queries",
               _bestURLPicker.getNextBaseURL(), DurationToString(_minDurationBetweenQueries));

    if (settings::IsProxyRequested(runMode)) {
      if (!IsProxyAvailable()) {
        throw std::runtime_error("Requesting proxy usage without any available proxy");
      }
      setUpProxy(GetProxyURL(), false);
    }
  }
}

/**
 * Should function remove proxy for subsequent calls when option is off ? not useful for now
 */
void CurlHandle::setUpProxy(const char *proxyUrl, bool reset) {
  if (proxyUrl != nullptr || reset) {
    log::info("Setting proxy to {} reset = {} ?", proxyUrl, reset);
    CURL *curl = reinterpret_cast<CURL *>(_handle);
    CurlSetLogIfError(curl, CURLOPT_PROXY, proxyUrl);  // Default of nullptr
    CurlSetLogIfError(curl, CURLOPT_CAINFO, GetProxyCAInfo());
    CurlSetLogIfError(curl, CURLOPT_SSL_VERIFYHOST, proxyUrl != nullptr ? 0L : 1L);
  }
}

std::string_view CurlHandle::query(std::string_view endpoint, const CurlOptions &opts) {
  const CurlPostData &postData = opts.postData();
  const bool queryResponseOverrideMode = _handle == nullptr;
  const bool appendParametersInQueryStr =
      !postData.empty() && (opts.requestType() != HttpRequestType::kPost || queryResponseOverrideMode);

  const int8_t baseUrlPos = _bestURLPicker.nextBaseURLPos();
  const std::string_view baseUrl = _bestURLPicker.getBaseURL(baseUrlPos);
  const std::string_view postDataStr = postData.str();
  string modifiedURL(baseUrl.size() + endpoint.size() + (appendParametersInQueryStr ? (1U + postDataStr.size()) : 0U),
                     '?');

  auto modifiedUrlOutIt = std::ranges::copy(baseUrl, modifiedURL.begin()).out;
  modifiedUrlOutIt = std::ranges::copy(endpoint, modifiedUrlOutIt).out;
  string jsonStr;  // Declared here as its scope should be valid until the actual curl call
  std::string_view optsStr;
  if (appendParametersInQueryStr) {
    modifiedUrlOutIt = std::ranges::copy(postDataStr, modifiedUrlOutIt + 1).out;
  } else if (opts.isPostDataInJsonFormat() && !postData.empty()) {
    jsonStr = postData.toJson().dump();
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

  CURL *curl = reinterpret_cast<CURL *>(_handle);

  CurlSetLogIfError(curl, CURLOPT_POSTFIELDS, optsStr.data());
  CurlSetLogIfError(curl, CURLOPT_URL, modifiedURL.c_str());

  // Important! We should reset ALL fields of curl object that may change for each call to query
  // as we don't reset curl options for each query
  CurlSetLogIfError(curl, CURLOPT_POST, opts.requestType() == HttpRequestType::kPost);
  CurlSetLogIfError(curl, CURLOPT_CUSTOMREQUEST, opts.requestType() == HttpRequestType::kDelete ? "DELETE" : nullptr);
  if (opts.requestType() == HttpRequestType::kGet) {
    // This is to force cURL to switch in a GET request
    // Useless to reset to 0 in other cases
    CurlSetLogIfError(curl, CURLOPT_HTTPGET, 1);
  }

  CurlSetLogIfError(curl, CURLOPT_VERBOSE, opts.isVerbose() ? 1L : 0L);

  curl_slist *curlListPtr = nullptr;
  curl_slist *oldCurlListPtr = nullptr;
  for (const auto &[httpHeaderKey, httpHeaderValue] : opts.httpHeaders()) {
    // Trick: HttpHeaders is actually a FlatKeyValueString with '\0' as header separator and ':' as key / value
    // separator. curl_slist_append expects a 'const char *' as HTTP header - it's possible here to just give the
    // pointer to the beginning of the key as we know the bundle key/value ends with a null-terminating char
    // (either there is at least one more key / value pair, either it's the last one and it's also fine as string is
    // guaranteed to be null-terminated since C++11)
    curlListPtr = curl_slist_append(curlListPtr, httpHeaderKey.data());
    if (curlListPtr == nullptr) {
      curl_slist_free_all(oldCurlListPtr);
      throw std::bad_alloc();
    }
    oldCurlListPtr = curlListPtr;
  }

  using CurlSlistDeleter = decltype([](curl_slist *hdrList) { curl_slist_free_all(hdrList); });
  using CurlListUniquePtr = std::unique_ptr<curl_slist, CurlSlistDeleter>;

  CurlListUniquePtr curlListUniquePtr(curlListPtr);

  CurlSetLogIfError(curl, CURLOPT_HTTPHEADER, curlListPtr);

  setUpProxy(opts.proxyUrl(), opts.isProxyReset());

  if (_minDurationBetweenQueries != Duration::zero()) {
    // Check last request time
    const auto nowTime = Clock::now();
    if (nowTime < _lastQueryTime + _minDurationBetweenQueries) {
      // We should sleep a bit before performing query
      const Duration sleepingTime = _minDurationBetweenQueries - (nowTime - _lastQueryTime);
      log::debug("Wait {} before performing query", DurationToString(sleepingTime));
      std::this_thread::sleep_for(sleepingTime);
      _lastQueryTime = nowTime + sleepingTime;
    } else {
      // Query can be performed immediately
      _lastQueryTime = nowTime;
    }
  }

  log::log(_requestCallLogLevel, "{} {}{}{}", ToString(opts.requestType()), modifiedURL, optsStr.empty() ? "" : "?",
           optsStr);

  // Actually make the query, with a fast retry mechanism
  Duration sleepingTime = milliseconds(100);
  int retryPos = 0;
  CURLcode res;

  _queryData.clear();

  do {
    if (retryPos != 0) {
      if (_pMetricGateway != nullptr) {
        _pMetricGateway->add(MetricType::kCounter, MetricOperation::kIncrement,
                             CurlMetrics::kNbRequestErrorKeys.find(opts.requestType())->second);
      }
      log::error("Got curl error ({}), retry {}/{} after {}", static_cast<int>(res), retryPos, _nbMaxRetries,
                 DurationToString(sleepingTime));
      std::this_thread::sleep_for(sleepingTime);
      sleepingTime *= 2;
    }

    auto t1 = Clock::now();

    // Call
    res = curl_easy_perform(curl);

    // Store stats
    const auto queryRTInMs = static_cast<uint32_t>(GetTimeFrom<milliseconds>(t1).count());
    _bestURLPicker.storeResponseTimePerBaseURL(baseUrlPos, queryRTInMs);

    if (_pMetricGateway != nullptr) {
      _pMetricGateway->add(MetricType::kCounter, MetricOperation::kIncrement,
                           CurlMetrics::kNbRequestsKeys.find(opts.requestType())->second);
      _pMetricGateway->add(MetricType::kHistogram, MetricOperation::kObserve,
                           CurlMetrics::kRequestDurationKeys.find(opts.requestType())->second,
                           static_cast<double>(queryRTInMs));
    }

    // Periodic memory release to avoid memory leak for a very large number of requests
    static constexpr int kReleaseMemoryRequestsFrequency = 1000;
    if ((_bestURLPicker.nbRequestsDone() % kReleaseMemoryRequestsFrequency) == 0) {
      _queryData.shrink_to_fit();
    }

  } while (res != CURLE_OK && ++retryPos <= _nbMaxRetries);
  if (retryPos > _nbMaxRetries) {
    switch (_tooManyErrorsPolicy) {
      case PermanentCurlOptions::TooManyErrorsPolicy::kReturnEmptyResponse:
        log::error("Too many errors from curl, return empty response");
        _queryData.clear();
        break;
      case PermanentCurlOptions::TooManyErrorsPolicy::kThrow:
        throw exception("Too many errors from curl, last ({})", static_cast<int>(res));
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
    log::log(_requestAnswerLogLevel, "Truncated non JSON response {}...", outPrinted);
  } else {
    log::log(_requestAnswerLogLevel, "Full{}JSON response {}", mayBeJsonResponse ? " " : " non ", _queryData);
  }

  return _queryData;
}

void CurlHandle::setOverridenQueryResponses(const std::map<string, string> &queryResponsesMap) {
  if (_handle != nullptr) {
    throw exception(
        "CurlHandle should be created in Query response override mode in order to override its next response");
  }
  FlatQueryResponseMap flatQueryResponses;
  for (const auto &[query, response] : queryResponsesMap) {
    flatQueryResponses.append(query, response);
  }
  _queryData = string(flatQueryResponses.str());
}

void CurlHandle::setWriteData() {
  if (_handle != nullptr) {
    CURL *curl = reinterpret_cast<CURL *>(_handle);
    CurlSetLogIfError(curl, CURLOPT_WRITEDATA, &_queryData);
  }
}

void CurlHandle::swap(CurlHandle &rhs) noexcept {
  using std::swap;

  swap(_handle, rhs._handle);
  swap(_pMetricGateway, rhs._pMetricGateway);
  swap(_minDurationBetweenQueries, rhs._minDurationBetweenQueries);
  swap(_lastQueryTime, rhs._lastQueryTime);
  swap(_bestURLPicker, rhs._bestURLPicker);
  _queryData.swap(rhs._queryData);
  swap(_requestCallLogLevel, rhs._requestCallLogLevel);
  swap(_requestAnswerLogLevel, rhs._requestAnswerLogLevel);
  swap(_nbMaxRetries, rhs._nbMaxRetries);
  swap(_tooManyErrorsPolicy, rhs._tooManyErrorsPolicy);

  setWriteData();
  rhs.setWriteData();
}

CurlHandle::CurlHandle(CurlHandle &&rhs) noexcept { swap(rhs); }

CurlHandle &CurlHandle::operator=(CurlHandle &&rhs) noexcept {
  swap(rhs);
  return *this;
}

CurlHandle::~CurlHandle() {
  if (_handle != nullptr) {
    curl_easy_cleanup(reinterpret_cast<CURL *>(_handle));
  }
}

CurlInitRAII::CurlInitRAII() {
  CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
  if (code != CURLE_OK) {
    std::ostringstream oss;
    oss << "curl_global_init() failed: " << curl_easy_strerror(code);
    throw std::runtime_error(oss.str());
  }
}

CurlInitRAII::~CurlInitRAII() { curl_global_cleanup(); }

}  // namespace cct
