#include "curlhandle.hpp"

#include <curl/curl.h>

#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_proxy.hpp"

namespace cct {

namespace {

size_t CurlWriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  reinterpret_cast<std::string *>(userp)->append(static_cast<const char *>(contents), size * nmemb);
  return size * nmemb;
}
}  // namespace

CurlHandle::CurlHandle(Clock::duration minDurationBetweenQueries, settings::RunMode runMode)
    : _handle(curl_easy_init()), _minDurationBetweenQueries(minDurationBetweenQueries) {
  if (!_handle) {
    throw std::bad_alloc();
  }
  CURL *curl = reinterpret_cast<CURL *>(_handle);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
  log::debug("Initialize a new CurlHandle with {} ms as minimum duration between queries",
             std::chrono::duration_cast<std::chrono::milliseconds>(minDurationBetweenQueries).count());

  if (IsProxyRequested(runMode)) {
    if (IsProxyAvailable()) {
      setUpProxy(CurlOptions::ProxySettings(false, GetProxyURL()));
    } else {
      throw std::runtime_error("Requesting proxy usage without any available proxy.");
    }
  }
}

CurlHandle::CurlHandle(CurlHandle &&o) noexcept
    : _handle(std::exchange(o._handle, nullptr)),
      _minDurationBetweenQueries(o._minDurationBetweenQueries),
      _lastQueryTime(std::exchange(o._lastQueryTime, TimePoint())) {}

CurlHandle &CurlHandle::operator=(CurlHandle &&o) noexcept {
  if (this != std::addressof(o)) {
    _handle = std::exchange(o._handle, nullptr);
    _minDurationBetweenQueries = o._minDurationBetweenQueries;
    _lastQueryTime = std::exchange(o._lastQueryTime, TimePoint());
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
void CurlHandle::setUpProxy(const CurlOptions::ProxySettings &proxy) {
  if (proxy._url || proxy._reset) {
    log::info("Setting proxy to {} reset = {} ?", proxy._url, proxy._reset);
    checkHandleOrInit();
    CURL *curl = reinterpret_cast<CURL *>(_handle);
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy._url);  // Default of nullptr
    curl_easy_setopt(curl, CURLOPT_CAINFO, GetProxyCAInfo());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, proxy._url ? 0L : 1L);
  }
}

std::string CurlHandle::query(std::string_view url, const CurlOptions &opts) {
  checkHandleOrInit();
  CURL *curl = reinterpret_cast<CURL *>(_handle);

  // General option settings.
  log::info("{} {}{}{}", opts.requestTypeStr(), url, opts.postdata.empty() ? "" : " opts ", opts.postdata.c_str());
  std::string modifiedURL(url);
  if (opts.requestType() != CurlOptions::RequestType::kPost && !opts.postdata.empty()) {
    // Add parameters as query string after the URL
    modifiedURL.push_back('?');
    modifiedURL.append(opts.postdata.toStringView());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
  } else {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, opts.postdata.c_str());
  }
  curl_easy_setopt(curl, CURLOPT_URL, modifiedURL.c_str());
  curl_easy_setopt(curl, CURLOPT_USERAGENT, opts.userAgent);

#ifdef _WIN32
  // https://stackoverflow.com/questions/37551409/configure-curl-to-use-default-system-cert-store-on-windows
  curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif

  // Important! We should reset ALL fields of curl object at each call to query,
  // as it would be possible for a new query to read from a dangling reference form a previous
  // query.
  curl_easy_setopt(curl, CURLOPT_POST, opts.requestType() == CurlOptions::RequestType::kPost);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,
                   opts.requestType() == CurlOptions::RequestType::kDelete ? "DELETE" : nullptr);
  if (opts.requestType() == CurlOptions::RequestType::kGet) {
    // This is to force cURL to switch in a GET request
    // Useless to reset to 0 in other cases
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
  }

  curl_easy_setopt(curl, CURLOPT_VERBOSE, opts.verbose ? 1L : 0L);
  curl_slist *curlListPtr = nullptr, *oldCurlListPtr = nullptr;
  for (const std::string &header : opts.httpHeaders) {
    curlListPtr = curl_slist_append(curlListPtr, header.c_str());
    if (!curlListPtr) {
      if (oldCurlListPtr) {
        curl_slist_free_all(oldCurlListPtr);
      }
      throw std::bad_alloc();
    }
    oldCurlListPtr = curlListPtr;
  }
  using CurlListUniquePtr =
      std::unique_ptr<curl_slist, decltype([](curl_slist *hdrList) { curl_slist_free_all(hdrList); })>;
  CurlListUniquePtr curlListUniquePtr(curlListPtr);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlListPtr);
  std::string out;
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);

  setUpProxy(opts.proxy);

  if (_minDurationBetweenQueries != Clock::duration::zero()) {
    // Check last request time
    const TimePoint t = Clock::now();
    if (t < _lastQueryTime + _minDurationBetweenQueries) {
      // We should sleep a bit before performing query
      const Clock::duration sleepingTime = _minDurationBetweenQueries - (t - _lastQueryTime);
      log::trace("Wait {} ms before performing query",
                 std::chrono::duration_cast<std::chrono::milliseconds>(sleepingTime).count());
      std::this_thread::sleep_for(sleepingTime);
      _lastQueryTime = t + sleepingTime;
    } else {
      // Query can be performed immediately
      _lastQueryTime = t;
    }
  }
  const CURLcode res = curl_easy_perform(curl);  // Get reply
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

  if (res != CURLE_OK) {
    throw exception("Unexpected response from curl: Error " + std::to_string(res));
  }
  return out;
}

void CurlHandle::UrlEncodeDeleter::operator()(char *ptr) const { curl_free(ptr); }

CurlHandle::CurlStringUniquePtr CurlHandle::urlEncode(const std::string &url) {
  CURL *curl = reinterpret_cast<CURL *>(_handle);
  return CurlStringUniquePtr(curl_easy_escape(curl, url.c_str(), static_cast<int>(url.size())));
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