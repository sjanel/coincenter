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
#include "flatkeyvaluestring.hpp"
#include "httprequesttype.hpp"
#include "metric.hpp"
#include "permanentcurloptions.hpp"
#include "proxy.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

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
    // This will cause CURL to raise an error
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
  curlVersionInfoStr.append(" ssl ").append(curlVersionInfo.ssl_version);
  curlVersionInfoStr.append(" libz ").append(curlVersionInfo.libz_version);
  return curlVersionInfoStr;
}

CurlHandle::CurlHandle(const BestURLPicker &bestURLPicker, AbstractMetricGateway *pMetricGateway,
                       const PermanentCurlOptions &permanentCurlOptions, settings::RunMode runMode)
    : _pMetricGateway(pMetricGateway),
      _minDurationBetweenQueries(permanentCurlOptions.minDurationBetweenQueries()),
      _bestUrlPicker(bestURLPicker),
      _requestCallLogLevel(permanentCurlOptions.requestCallLogLevel()),
      _requestAnswerLogLevel(permanentCurlOptions.requestAnswerLogLevel()) {
  if (settings::AreQueryResponsesOverriden(runMode)) {
    _handle = nullptr;
  } else {
    _handle = curl_easy_init();
    if (_handle == nullptr) {
      throw std::bad_alloc();
    }
    CURL *curl = reinterpret_cast<CURL *>(_handle);

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

    log::debug("Initialize CurlHandle for {} with {} ms as minimum duration between queries",
               bestURLPicker.getNextBaseURL(),
               std::chrono::duration_cast<TimeInMs>(_minDurationBetweenQueries).count());

    if (settings::IsProxyRequested(runMode)) {
      if (IsProxyAvailable()) {
        setUpProxy(GetProxyURL(), false);
      } else {
        throw std::runtime_error("Requesting proxy usage without any available proxy.");
      }
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
  const CurlPostData &postData = opts.getPostData();
  const bool queryResponseOverrideMode = _handle == nullptr;
  const bool appendParametersInQueryStr =
      !postData.empty() && (opts.requestType() != HttpRequestType::kPost || queryResponseOverrideMode);

  const int8_t baseUrlPos = _bestUrlPicker.nextBaseURLPos();
  const std::string_view baseUrl = _bestUrlPicker.getBaseURL(baseUrlPos);
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
  for (const auto &[httpHeaderKey, httpHeaderValue] : opts.getHttpHeaders()) {
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

  setUpProxy(opts.getProxyUrl(), opts.isProxyReset());

  _queryData.clear();

  if (_minDurationBetweenQueries != Duration::zero()) {
    // Check last request time
    const TimePoint nowTime = Clock::now();
    if (nowTime < _lastQueryTime + _minDurationBetweenQueries) {
      // We should sleep a bit before performing query
      const Duration sleepingTime = _minDurationBetweenQueries - (nowTime - _lastQueryTime);
      log::debug("Wait {} ms before performing query", std::chrono::duration_cast<TimeInMs>(sleepingTime).count());
      std::this_thread::sleep_for(sleepingTime);
      _lastQueryTime = nowTime + sleepingTime;
    } else {
      // Query can be performed immediately
      _lastQueryTime = nowTime;
    }
  }

  log::log(_requestCallLogLevel, "{} {}{}{}", ToString(opts.requestType()), modifiedURL, optsStr.empty() ? "" : "?",
           optsStr);

  // Actually make the query
  const TimePoint t1 = Clock::now();
  const CURLcode res = curl_easy_perform(curl);  // Get reply
  if (res != CURLE_OK) {
    throw exception("Unexpected response from curl: Error {}", static_cast<int>(res));
  }

  // Store stats
  const uint32_t queryRTInMs = static_cast<uint32_t>(GetTimeFrom<TimeInMs>(t1).count());
  _bestUrlPicker.storeResponseTimePerBaseURL(baseUrlPos, queryRTInMs);

  static constexpr int kReleaseMemoryRequestsFrequency = 100;

  // Periodic memory release to avoid memory leak for a very large number of requests
  if ((_bestUrlPicker.nbRequestsDone() % kReleaseMemoryRequestsFrequency) == 0) {
    _queryData.shrink_to_fit();
  }

  if (_pMetricGateway != nullptr) {
    _pMetricGateway->add(MetricType::kCounter, MetricOperation::kIncrement,
                         CurlMetrics::kNbRequestsKeys.find(opts.requestType())->second);
    _pMetricGateway->add(MetricType::kHistogram, MetricOperation::kObserve,
                         CurlMetrics::kRequestDurationKeys.find(opts.requestType())->second,
                         static_cast<double>(queryRTInMs));
  }

  // Avoid polluting the logs for large response which are more likely to be HTML
  const bool isJsonAnswer = _queryData.starts_with('{') || _queryData.starts_with('[');
  static constexpr std::size_t kMaxLenResponse = 1000;
  if (!isJsonAnswer && _queryData.size() > kMaxLenResponse) {
    const std::string_view outPrinted(_queryData.begin(),
                                      _queryData.begin() + std::min(_queryData.size(), kMaxLenResponse));
    log::log(_requestAnswerLogLevel, "Truncated non JSON response {}...", outPrinted);
  } else {
    log::log(_requestAnswerLogLevel, "Full{}JSON response {}", isJsonAnswer ? " " : " non ", _queryData);
  }

  return _queryData;
}

string CurlHandle::queryRelease(std::string_view endpoint, const CurlOptions &opts) {
  query(endpoint, opts);
  string queryData;
  queryData.swap(_queryData);
  return queryData;
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
