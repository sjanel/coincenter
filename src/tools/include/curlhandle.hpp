#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include "cct_run_modes.hpp"
#include "cct_vector.hpp"
#include "curloptions.hpp"

namespace cct {

/// RAII class managing safely a CURL handle.
///
/// Aim of this class is to simplify curl library complexity usage, and abstracts it from client
///
/// Note that this implementation is not thread-safe. It is recommended to embed an instance of
/// CurlHandle for faster similar queries.
class CurlHandle {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  /// Constructs a new CurlHandle.
  /// @param minDurationBetweenQueries delay query 'n + 1' in case query 'n' was too close
  explicit CurlHandle(Clock::duration minDurationBetweenQueries = Clock::duration::zero(),
                      settings::RunMode runMode = settings::kProd);

  CurlHandle(const CurlHandle &) = delete;
  CurlHandle &operator=(const CurlHandle &) = delete;

  CurlHandle(CurlHandle &&o) noexcept;
  CurlHandle &operator=(CurlHandle &&o) noexcept;

  struct UrlEncodeDeleter {
    void operator()(char *ptr) const;
  };

  using CurlStringUniquePtr = std::unique_ptr<char, UrlEncodeDeleter>;

  CurlStringUniquePtr urlEncode(const std::string &url);

  std::string query(std::string_view url, const CurlOptions &opts);

  Clock::duration minDurationBetweenQueries() const { return _minDurationBetweenQueries; }

  ~CurlHandle();

 private:
  void checkHandleOrInit();
  void setUpProxy(const CurlOptions::ProxySettings &proxy);
  // void pointer instead of CURL to avoid having to forward declare (we don't know about the underlying definition)
  // and to avoid clients to pull unnecessary curl dependencies by just including the header
  void *_handle;
  Clock::duration _minDurationBetweenQueries;
  TimePoint _lastQueryTime;
};

class CurlInitRAII {
 public:
  CurlInitRAII();

  CurlInitRAII(const CurlInitRAII &) = delete;
  CurlInitRAII &operator=(const CurlInitRAII &) = delete;
  CurlInitRAII(CurlInitRAII &&) = delete;
  CurlInitRAII &operator=(CurlInitRAII &&) = delete;

  ~CurlInitRAII();
};
}  // namespace cct