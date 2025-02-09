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
#include "write-json.hpp"

namespace cct {

/// Utility class to factorize basic retry mechanism around curlHandle query.
/// Request options remain constant during calls.
class RequestRetry {
 public:
  enum class Status : int8_t { kResponseError, kResponseOK };

 private:
  static constexpr auto kDefaultJsonOpts =
      json::opts{.error_on_unknown_keys = false,  // NOLINT(readability-implicit-bool-conversion)
                 .minified = true,                // NOLINT(readability-implicit-bool-conversion)
                 .error_on_const_read = true,     // NOLINT(readability-implicit-bool-conversion)
                 .raw_string = true};             // NOLINT(readability-implicit-bool-conversion)

 public:
  RequestRetry(CurlHandle &curlHandle, CurlOptions curlOptions, QueryRetryPolicy queryRetryPolicy = QueryRetryPolicy())
      : _curlHandle(curlHandle), _curlOptions(std::move(curlOptions)), _queryRetryPolicy(queryRetryPolicy) {}

  template <class T, json::opts opts = kDefaultJsonOpts>
  T query(const auto &endpoint, auto responseStatus) {
    return query<T, opts>(endpoint, responseStatus, [](CurlOptions &) {});
  }

  template <class T, json::opts opts = kDefaultJsonOpts>
  T query(const auto &endpoint, auto responseStatus, auto postDataUpdateFunc) {
    auto sleepingTime = _queryRetryPolicy.initialRetryDelay;
    decltype(_queryRetryPolicy.nbMaxRetries) nbRetries = 0;
    bool parsingError;

    T ret{};

    do {
      if (nbRetries != 0) {
        if (log::get_level() <= log::level::warn) {
          log::warn("Got query error: '{}' for {}, retry {}/{} after {}", WriteJsonOrThrow(ret), endpoint, nbRetries,
                    _queryRetryPolicy.nbMaxRetries, DurationToString(sleepingTime));
        }

        std::this_thread::sleep_for(sleepingTime);
        sleepingTime *= _queryRetryPolicy.exponentialBackoff;
      }

      postDataUpdateFunc(_curlOptions);

      auto queryStrRes = _curlHandle.query(endpoint, _curlOptions);
      auto ec = json::read<opts>(ret, queryStrRes);
      if (ec) {
        auto prefixJsonContent = queryStrRes.substr(0, std::min<int>(queryStrRes.size(), 20));
        log::error("Error while reading json content '{}{}': {}", prefixJsonContent,
                   prefixJsonContent.size() < queryStrRes.size() ? "..." : "", json::format_error(ec, queryStrRes));
        parsingError = true;
      } else {
        parsingError = false;
      }

    } while ((parsingError || responseStatus(ret) == Status::kResponseError) &&
             ++nbRetries <= _queryRetryPolicy.nbMaxRetries);

    if (nbRetries > _queryRetryPolicy.nbMaxRetries) {
      switch (_queryRetryPolicy.tooManyFailuresPolicy) {
        case QueryRetryPolicy::TooManyFailuresPolicy::kReturnEmpty:
          log::error("Too many query errors, returning value initialized object");
          ret = T();
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