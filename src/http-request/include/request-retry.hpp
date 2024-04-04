#pragma once

#include <cstdint>
#include <thread>
#include <utility>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_type_traits.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "durationstring.hpp"
#include "query-retry-policy.hpp"
#include "timedef.hpp"
#include "unreachable.hpp"

namespace cct {

/// Utility class to factorize basic retry mechanism around curlHandle query.
/// Request options remain constant during calls.
class RequestRetry {
 public:
  enum class Status : int8_t { kResponseError, kResponseOK };

  RequestRetry(CurlHandle &curlHandle, CurlOptions curlOptions, QueryRetryPolicy queryRetryPolicy = QueryRetryPolicy())
      : _curlHandle(curlHandle), _curlOptions(std::move(curlOptions)), _queryRetryPolicy(queryRetryPolicy) {}

  /// Perform the query at most _nbMaxRetries + 1 times with an exponential backoff delay as long as
  /// responseStatus(jsonResponse) returns kResponseError.
  /// responseStatus should be a functor taking a single json by const reference argument, returning Status::kResponseOK
  /// if success, Status::kResponseError in case of error.
  template <class StringType, class ResponseStatusT>
  json queryJson(const StringType &endpoint, ResponseStatusT responseStatus) {
    return queryJson(endpoint, responseStatus, [](CurlOptions &) {});
  }

  /// Perform the query at most _nbMaxRetries + 1 times with an exponential backoff delay as long as
  /// responseStatus(jsonResponse) returns kResponseError.
  /// responseStatus should be a functor taking a single json by const reference argument, returning Status::kResponseOK
  /// if success, Status::kResponseError in case of error.
  /// postDataUpdateFunc is a functor that takes the embedded CurlOptions's reference as single argument and updates it
  /// before each query
  template <class StringType, class ResponseStatusT, class PostDataFuncT>
  json queryJson(const StringType &endpoint, ResponseStatusT responseStatus, PostDataFuncT postDataUpdateFunc) {
    decltype(_queryRetryPolicy.nbMaxRetries) nbRetries = 0;
    auto sleepingTime = _queryRetryPolicy.initialRetryDelay;
    json ret;

    do {
      if (nbRetries != 0) {
        log::warn("Got query error: '{}' for {}, retry {}/{} after {}", ret.dump(), endpoint, nbRetries,
                  _queryRetryPolicy.nbMaxRetries, DurationToString(sleepingTime));
        std::this_thread::sleep_for(sleepingTime);
        sleepingTime *= _queryRetryPolicy.exponentialBackoff;
      }

      postDataUpdateFunc(_curlOptions);

      static constexpr bool kAllowExceptions = false;
      ret = json::parse(_curlHandle.query(endpoint, _curlOptions), nullptr, kAllowExceptions);

    } while ((ret.is_discarded() || responseStatus(ret) == Status::kResponseError) &&
             ++nbRetries <= _queryRetryPolicy.nbMaxRetries);

    if (nbRetries > _queryRetryPolicy.nbMaxRetries) {
      switch (_queryRetryPolicy.tooManyFailuresPolicy) {
        case QueryRetryPolicy::TooManyFailuresPolicy::kReturnEmpty:
          log::error("Too many query errors, returning empty result");
          ret = json::object();
          break;
        case QueryRetryPolicy::TooManyFailuresPolicy::kThrowException:
          throw exception("Too many query errors");
        default:
          unreachable();
      }
    }

    return ret;
  }

  using trivially_relocatable = is_trivially_relocatable<CurlOptions>::type;

 private:
  CurlHandle &_curlHandle;
  CurlOptions _curlOptions;
  QueryRetryPolicy _queryRetryPolicy;
};

}  // namespace cct