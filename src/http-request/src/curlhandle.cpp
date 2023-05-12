#include "curlhandle.hpp"

#include <curl/curl.h>

#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

#include "abstractmetricgateway.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "curlmetrics.hpp"
#include "curloptions.hpp"
#include "proxy.hpp"
#include "stringhelpers.hpp"

namespace cct {

namespace {

size_t CurlWriteCallback(const char *contents, size_t size, size_t nmemb, void *userp) {
  try {
    reinterpret_cast<string *>(userp)->append(contents, size * nmemb);
  } catch (const std::bad_alloc &e) {
    // Do not throw exceptions in a function passed to a C library
    // This will cause CURL to raise an error
    log::error("Bad alloc catched in curl write call back action, returning 0: {}", e.what());
    return 0;
  }
  return size * nmemb;
}

template <class T>
void CurlSetLogIfError(CURL *curl, CURLoption curlOption, T value) {
  static_assert(std::is_integral_v<T> || std::is_pointer_v<T>);
  CURLcode code = curl_easy_setopt(curl, curlOption, value);
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
                       Duration minDurationBetweenQueries, settings::RunMode runMode)
    : _handle(curl_easy_init()),
      _pMetricGateway(pMetricGateway),
      _minDurationBetweenQueries(minDurationBetweenQueries),
      _bestUrlPicker(bestURLPicker) {
  if (_handle == nullptr) {
    throw std::bad_alloc();
  }
  CURL *curl = reinterpret_cast<CURL *>(_handle);
  CurlSetLogIfError(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
  log::debug("Initialize CurlHandle for {} with {} ms as minimum duration between queries",
             bestURLPicker.getNextBaseURL(), std::chrono::duration_cast<TimeInMs>(minDurationBetweenQueries).count());

  if (IsProxyRequested(runMode)) {
    if (IsProxyAvailable()) {
      setUpProxy(GetProxyURL(), false);
    } else {
      throw std::runtime_error("Requesting proxy usage without any available proxy.");
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

string CurlHandle::query(std::string_view endpoint, const CurlOptions &opts) {
  CURL *curl = reinterpret_cast<CURL *>(_handle);

  // General option settings.
  const CurlPostData &postData = opts.getPostData();
  const char *optsStr = postData.c_str();

  int8_t baseUrlPos = _bestUrlPicker.nextBaseURLPos();
  string modifiedURL(_bestUrlPicker.getBaseURL(baseUrlPos));
  modifiedURL.append(endpoint);
  string jsonBuf;  // Declared here as its scope should be valid until the actual curl call
  if (opts.requestType() != HttpRequestType::kPost && !postData.empty()) {
    // Add parameters as query string after the URL
    modifiedURL.push_back('?');
    modifiedURL.append(postData.str());
    optsStr = "";
  } else {
    if (opts.isPostDataInJsonFormat() && !postData.empty()) {
      jsonBuf = postData.toJson().dump();
      optsStr = jsonBuf.c_str();
    }
  }

  CurlSetLogIfError(curl, CURLOPT_POSTFIELDS, optsStr);
  CurlSetLogIfError(curl, CURLOPT_URL, modifiedURL.c_str());
  CurlSetLogIfError(curl, CURLOPT_USERAGENT, opts.getUserAgent());
  CurlSetLogIfError(curl, CURLOPT_FOLLOWLOCATION, opts.isFollowLocation());

#ifdef CCT_MSVC
  // https://stackoverflow.com/questions/37551409/configure-curl-to-use-default-system-cert-store-on-windows
  CurlSetLogIfError(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif

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
  using CurlListUniquePtr =
      std::unique_ptr<curl_slist, decltype([](curl_slist *hdrList) { curl_slist_free_all(hdrList); })>;
  CurlListUniquePtr curlListUniquePtr(curlListPtr);

  CurlSetLogIfError(curl, CURLOPT_HTTPHEADER, curlListPtr);
  string responseStr;
  CurlSetLogIfError(curl, CURLOPT_WRITEDATA, &responseStr);

  setUpProxy(opts.getProxyUrl(), opts.isProxyReset());

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

  log::info("{} {}{}{}", ToString(opts.requestType()), modifiedURL, optsStr[0] == '\0' ? "" : "?", optsStr);

  // Actually make the query
  TimePoint t1 = Clock::now();
  const CURLcode res = curl_easy_perform(curl);  // Get reply
  if (res != CURLE_OK) {
    throw exception("Unexpected response from curl: Error {}", static_cast<int>(res));
  }

  uint32_t queryRTInMs = static_cast<uint32_t>(GetTimeFrom<TimeInMs>(t1).count());
  _bestUrlPicker.storeResponseTimePerBaseURL(baseUrlPos, queryRTInMs);

  if (_pMetricGateway != nullptr) {
    _pMetricGateway->add(MetricType::kCounter, MetricOperation::kIncrement,
                         CurlMetrics::kNbRequestsKeys.find(opts.requestType())->second);
    _pMetricGateway->add(MetricType::kHistogram, MetricOperation::kObserve,
                         CurlMetrics::kRequestDurationKeys.find(opts.requestType())->second,
                         static_cast<double>(queryRTInMs));
  }

  if (log::get_level() <= log::level::trace) {
    // Avoid polluting the logs for large response which are more likely to be HTML
    const bool isJsonAnswer = responseStr.starts_with('{') || responseStr.starts_with('[');
    constexpr std::size_t kMaxLenResponse = 1000;
    if (!isJsonAnswer && responseStr.size() > kMaxLenResponse) {
      std::string_view outPrinted(responseStr.begin(),
                                  responseStr.begin() + std::min(responseStr.size(), kMaxLenResponse));
      log::trace("Truncated non JSON response {}...", outPrinted);
    } else {
      log::trace("Full{}JSON response {}", isJsonAnswer ? " " : " non ", responseStr);
    }
  }

  return responseStr;
}

string CurlHandle::urlEncode(std::string_view data) const {
  CURL *curl = reinterpret_cast<CURL *>(_handle);

  using CurlStringUniquePtr = std::unique_ptr<char, decltype([](char *ptr) { curl_free(ptr); })>;

  CurlStringUniquePtr uniquePtr(curl_easy_escape(curl, data.data(), static_cast<int>(data.size())));
  const char *encodedChars = uniquePtr.get();
  if (encodedChars == nullptr) {
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
