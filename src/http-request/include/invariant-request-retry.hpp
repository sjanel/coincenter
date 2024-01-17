#pragma once

#include <cstdint>
#include <string_view>
#include <thread>
#include <utility>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_type_traits.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "durationstring.hpp"
#include "timedef.hpp"
#include "unreachable.hpp"

namespace cct {

struct QueryRetryPolicy {
  enum class TooManyFailuresPolicy : int8_t { kReturnEmpty, kThrowException };

  Duration initialRetryDelay{TimeInMs(500)};
  float exponentialBackoff{2};
  int16_t nbMaxRetries{5};
  TooManyFailuresPolicy tooManyFailuresPolicy{TooManyFailuresPolicy::kReturnEmpty};
};

/// Utility class to factorize basic retry mechanism around curlHandle query.
/// Request options remain constant during calls.
class InvariantRequestRetry {
 public:
  InvariantRequestRetry(CurlHandle &curlHandle, std::string_view endpoint, CurlOptions curlOptions,
                        QueryRetryPolicy queryRetryPolicy = QueryRetryPolicy())
      : _curlHandle(curlHandle),
        _endpoint(endpoint),
        _curlOptions(std::move(curlOptions)),
        _queryRetryPolicy(queryRetryPolicy) {}

  enum class Status : int8_t { kResponseError, kResponseOK };

  /// Perform the query at most _nbMaxRetries + 1 times with an exponential backoff delay as long as
  /// responseStatus(jsonResponse) returns kResponseError.
  /// responseStatus should be a functor taking a single json by const reference argument, returning Status::kResponseOK
  /// if success, Status::kResponseError in case of error.
  template <class ResponseStatusT>
  json queryJson(ResponseStatusT responseStatus) {
    decltype(_queryRetryPolicy.nbMaxRetries) nbRetries = 0;
    auto sleepingTime = _queryRetryPolicy.initialRetryDelay;
    json ret;

    do {
      if (nbRetries != 0) {
        log::warn("Got query error: '{}', retry {}/{} after {}", ret.dump(), nbRetries, _queryRetryPolicy.nbMaxRetries,
                  DurationToString(sleepingTime));
        std::this_thread::sleep_for(sleepingTime);
        sleepingTime *= _queryRetryPolicy.exponentialBackoff;
      }
      ret = json::parse(_curlHandle.query(_endpoint, _curlOptions));
    } while (responseStatus(ret) == Status::kResponseError && ++nbRetries <= _queryRetryPolicy.nbMaxRetries);

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
  std::string_view _endpoint;
  CurlOptions _curlOptions;
  QueryRetryPolicy _queryRetryPolicy;
};
}  // namespace cct