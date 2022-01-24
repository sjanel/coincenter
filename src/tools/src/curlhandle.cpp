#include "curlhandle.hpp"

#include <curl/curl.h>

#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

#include "abstractmetricgateway.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_proxy.hpp"
#include "curlmetrics.hpp"
#include "stringhelpers.hpp"
#include "timehelpers.hpp"

namespace cct {

namespace {

size_t CurlWriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  try {
    reinterpret_cast<string *>(userp)->append(static_cast<const char *>(contents), size * nmemb);
    return size * nmemb;
  } catch (const std::bad_alloc &e) {
    // Do not throw exceptions in a function passed to a C library
    // This will cause CURL to raise an error
    log::error("Bad alloc catched in curl write call back action, returning 0: {}", e.what());
    return 0;
  }
}

template <class T>
void CurlSetLogIfError(CURL *curl, CURLoption curlOption, T value) {
  static_assert(std::is_integral_v<T> || std::is_pointer_v<T>);
  CURLcode code = curl_easy_setopt(curl, curlOption, value);
  if (code != CURLE_OK) {
    if constexpr (std::is_integral_v<T> || std::is_same_v<T, const char *>) {
      log::error("Curl error {} setting option {} to {}", code, curlOption, value);
    } else {
      log::error("Curl error {} setting option {}", code, curlOption);
    }
  }
}
}  // namespace

CurlHandle::CurlHandle(AbstractMetricGateway *pMetricGateway, Duration minDurationBetweenQueries,
                       settings::RunMode runMode)
    : _handle(curl_easy_init()),
      _pMetricGateway(pMetricGateway),
      _minDurationBetweenQueries(minDurationBetweenQueries) {
  if (!_handle) {
    throw std::bad_alloc();
  }
  CURL *curl = reinterpret_cast<CURL *>(_handle);
  CurlSetLogIfError(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
  log::debug("Initialize a new CurlHandle with {} ms as minimum duration between queries",
             std::chrono::duration_cast<std::chrono::milliseconds>(minDurationBetweenQueries).count());

  if (IsProxyRequested(runMode)) {
    if (IsProxyAvailable()) {
      setUpProxy(GetProxyURL(), false);
    } else {
      throw std::runtime_error("Requesting proxy usage without any available proxy.");
    }
  }
}

CurlHandle::CurlHandle(CurlHandle &&o) noexcept
    : _handle(std::exchange(o._handle, nullptr)),
      _pMetricGateway(o._pMetricGateway),
      _minDurationBetweenQueries(o._minDurationBetweenQueries),
      _lastQueryTime(o._lastQueryTime) {}

CurlHandle &CurlHandle::operator=(CurlHandle &&o) noexcept {
  if (this != std::addressof(o)) {
    _handle = std::exchange(o._handle, nullptr);
    _pMetricGateway = o._pMetricGateway;
    _minDurationBetweenQueries = o._minDurationBetweenQueries;
    _lastQueryTime = o._lastQueryTime;
  }
  return *this;
}

void CurlHandle::checkHandleOrInit() {
  if (!_handle) {
    _handle = curl_easy_init();
    if (!_handle) {
      throw std::bad_alloc();
    }
  }
}

/**
 * Should function remove proxy for subsequent calls when option is off ? not useful for now
 */
void CurlHandle::setUpProxy(const char *proxyUrl, bool reset) {
  if (proxyUrl || reset) {
    log::info("Setting proxy to {} reset = {} ?", proxyUrl, reset);
    checkHandleOrInit();
    CURL *curl = reinterpret_cast<CURL *>(_handle);
    CurlSetLogIfError(curl, CURLOPT_PROXY, proxyUrl);  // Default of nullptr
    CurlSetLogIfError(curl, CURLOPT_CAINFO, GetProxyCAInfo());
    CurlSetLogIfError(curl, CURLOPT_SSL_VERIFYHOST, proxyUrl ? 0L : 1L);
  }
}

string CurlHandle::query(std::string_view url, const CurlOptions &opts) {
  checkHandleOrInit();
  CURL *curl = reinterpret_cast<CURL *>(_handle);

  // General option settings.
  const CurlPostData &postData = opts.getPostData();
  const char *optsStr = postData.c_str();

  string modifiedURL(url);
  string jsonBuf;  // Declared here as its scope should be valid until the actual curl call
  if (opts.requestType() != HttpRequestType::kPost && !postData.empty()) {
    // Add parameters as query string after the URL
    modifiedURL.push_back('?');
    modifiedURL.append(postData.str());
    CurlSetLogIfError(curl, CURLOPT_POSTFIELDS, "");
  } else {
    if (opts.isPostDataInJsonFormat() && !postData.empty()) {
      jsonBuf = postData.toJson().dump();
      optsStr = jsonBuf.c_str();
    }
    CurlSetLogIfError(curl, CURLOPT_POSTFIELDS, optsStr);
  }

  CurlSetLogIfError(curl, CURLOPT_URL, modifiedURL.c_str());
  CurlSetLogIfError(curl, CURLOPT_USERAGENT, opts.getUserAgent());
  CurlSetLogIfError(curl, CURLOPT_FOLLOWLOCATION, opts.isFollowLocation());

#ifdef _MSC_VER
  // https://stackoverflow.com/questions/37551409/configure-curl-to-use-default-system-cert-store-on-windows
  CurlSetLogIfError(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif

  // Important! We should reset ALL fields of curl object at each call to query,
  // as it would be possible for a new query to read from a dangling reference form a previous
  // query.
  CurlSetLogIfError(curl, CURLOPT_POST, opts.requestType() == HttpRequestType::kPost);
  CurlSetLogIfError(curl, CURLOPT_CUSTOMREQUEST, opts.requestType() == HttpRequestType::kDelete ? "DELETE" : nullptr);
  if (opts.requestType() == HttpRequestType::kGet) {
    // This is to force cURL to switch in a GET request
    // Useless to reset to 0 in other cases
    CurlSetLogIfError(curl, CURLOPT_HTTPGET, 1);
  }

  CurlSetLogIfError(curl, CURLOPT_VERBOSE, opts.isVerbose() ? 1L : 0L);
  curl_slist *curlListPtr = nullptr, *oldCurlListPtr = nullptr;
  for (const auto &[httpHeaderKey, httpHeaderValue] : opts.getHttpHeaders()) {
    // Trick: HttpHeaders is actually a FlatKeyValueString with '\0' as header separator and ':' as key / value
    // separator. curl_slist_append expects a 'const char *' as HTTP header - it's possible here to just give the
    // pointer to the beginning of the key as we know the bundle key/value ends with a null-terminating char
    // (either there is at least one more key / value pair, either it's the last one and it's also fine as string is
    // guaranteed to be null-terminated since C++11)
    curlListPtr = curl_slist_append(curlListPtr, httpHeaderKey.data());
    if (!curlListPtr) {
      curl_slist_free_all(oldCurlListPtr);
      throw std::bad_alloc();
    }
    oldCurlListPtr = curlListPtr;
  }
  using CurlListUniquePtr =
      std::unique_ptr<curl_slist, decltype([](curl_slist *hdrList) { curl_slist_free_all(hdrList); })>;
  CurlListUniquePtr curlListUniquePtr(curlListPtr);

  CurlSetLogIfError(curl, CURLOPT_HTTPHEADER, curlListPtr);
  string out;
  CurlSetLogIfError(curl, CURLOPT_WRITEDATA, &out);

  setUpProxy(opts.getProxyUrl(), opts.isProxyReset());

  if (_minDurationBetweenQueries != Duration::zero()) {
    // Check last request time
    const TimePoint t = Clock::now();
    if (t < _lastQueryTime + _minDurationBetweenQueries) {
      // We should sleep a bit before performing query
      const Duration sleepingTime = _minDurationBetweenQueries - (t - _lastQueryTime);
      log::debug("Wait {} ms before performing query",
                 std::chrono::duration_cast<std::chrono::milliseconds>(sleepingTime).count());
      std::this_thread::sleep_for(sleepingTime);
      _lastQueryTime = t + sleepingTime;
    } else {
      // Query can be performed immediately
      _lastQueryTime = t;
    }
  }

  log::info("{} {}{}{}", ToString(opts.requestType()), url, optsStr[0] == '\0' ? "" : "?", optsStr);

  // Actually make the query
  TimePoint t1 = Clock::now();
  const CURLcode res = curl_easy_perform(curl);  // Get reply
  if (res != CURLE_OK) {
    string ex("Unexpected response from curl: Error ");
    AppendString(ex, static_cast<int>(res));
    throw exception(std::move(ex));
  }

  if (_pMetricGateway) {
    _pMetricGateway->add(MetricType::kCounter, MetricOperation::kIncrement,
                         CurlMetrics::kNbRequestsKeys.find(opts.requestType())->second);
    _pMetricGateway->add(MetricType::kHistogram, MetricOperation::kObserve,
                         CurlMetrics::kRequestDurationKeys.find(opts.requestType())->second,
                         static_cast<double>(GetTimeFrom<std::chrono::milliseconds>(t1).count()) / 1000);
  }

  if (log::get_level() <= log::level::trace) {
    // Avoid polluting the logs for large response which are more likely to be HTML
    const bool isJsonAnswer = out.starts_with('{') || out.starts_with('[');
    constexpr std::size_t kMaxLenResponse = 1000;
    if (!isJsonAnswer && out.size() > kMaxLenResponse) {
      std::string_view outPrinted(out.begin(), out.begin() + std::min(out.size(), kMaxLenResponse));
      log::trace("Truncated non JSON response {}...", outPrinted);
    } else {
      log::trace("Full{}JSON response {}", isJsonAnswer ? " " : " non ", out);
    }
  }

  return out;
}

string CurlHandle::urlEncode(std::string_view url) const {
  CURL *curl = reinterpret_cast<CURL *>(_handle);

  using CurlStringUniquePtr = std::unique_ptr<char, decltype([](char *ptr) { curl_free(ptr); })>;

  CurlStringUniquePtr uniquePtr(curl_easy_escape(curl, url.data(), static_cast<int>(url.size())));
  const char *encodedChars = uniquePtr.get();
  if (!encodedChars) {
    throw std::bad_alloc();
  }
  return encodedChars;
}

CurlHandle::~CurlHandle() { curl_easy_cleanup(reinterpret_cast<CURL *>(_handle)); }

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
