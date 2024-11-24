#pragma once

#include <cstdint>
#include <thread>
#include <utility>

#include "cct_exception.hpp"
#include "cct_json-container.hpp"
#include "cct_json-serialization.hpp"
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

  RequestRetry(CurlHandle &curlHandle, CurlOptions curlOptions, QueryRetryPolicy queryRetryPolicy = QueryRetryPolicy())
      : _curlHandle(curlHandle), _curlOptions(std::move(curlOptions)), _queryRetryPolicy(queryRetryPolicy) {}

  /// Perform the query at most _nbMaxRetries + 1 times with an exponential backoff delay as long as
  /// responseStatus(jsonResponse) returns kResponseError.
  /// responseStatus should be a functor taking a single json by const reference argument, returning Status::kResponseOK
  /// if success, Status::kResponseError in case of error.
  template <class StringType, class ResponseStatusT>
  json::container queryJson(const StringType &endpoint, ResponseStatusT responseStatus) {
    return queryJson(endpoint, responseStatus, [](CurlOptions &) {});
  }

  /// Perform the query at most _nbMaxRetries + 1 times with an exponential backoff delay as long as
  /// responseStatus(jsonResponse) returns kResponseError.
  /// responseStatus should be a functor taking a single json by const reference argument, returning Status::kResponseOK
  /// if success, Status::kResponseError in case of error.
  /// postDataUpdateFunc is a functor that takes the embedded CurlOptions's reference as single argument and updates it
  /// before each query
  template <class StringType, class ResponseStatusT, class PostDataFuncT>
  json::container queryJson(const StringType &endpoint, ResponseStatusT responseStatus,
                            PostDataFuncT postDataUpdateFunc) {
    return query<json::container, json::opts{}, StringType, ResponseStatusT, PostDataFuncT>(endpoint, responseStatus,
                                                                                            postDataUpdateFunc);
  }

  template <class T, json::opts opts, class StringType, class ResponseStatusT>
  T query(const StringType &endpoint, ResponseStatusT responseStatus) {
    return query<T, opts>(endpoint, responseStatus, [](CurlOptions &) {});
  }

  template <class T, json::opts opts, class StringType, class ResponseStatusT, class PostDataFuncT>
  T query(const StringType &endpoint, ResponseStatusT responseStatus, PostDataFuncT postDataUpdateFunc) {
    auto sleepingTime = _queryRetryPolicy.initialRetryDelay;
    decltype(_queryRetryPolicy.nbMaxRetries) nbRetries = 0;
    bool parsingError;
    T ret;

    do {
      if (nbRetries != 0) {
        if (log::get_level() <= log::level::warn) {
          string strContent;
          if constexpr (std::is_same_v<T, json::container>) {
            strContent = ret.dump();
          } else {
            strContent = WriteMiniJsonOrThrow(ret);
          }
          log::warn("Got query error: '{}' for {}, retry {}/{} after {}", strContent, endpoint, nbRetries,
                    _queryRetryPolicy.nbMaxRetries, DurationToString(sleepingTime));
        }

        std::this_thread::sleep_for(sleepingTime);
        sleepingTime *= _queryRetryPolicy.exponentialBackoff;
      }

      postDataUpdateFunc(_curlOptions);

      auto queryStrRes = _curlHandle.query(endpoint, _curlOptions);
      if constexpr (std::is_same_v<T, json::container>) {
        static constexpr bool kAllowExceptions = false;
        ret = json::container::parse(queryStrRes, nullptr, kAllowExceptions);
        parsingError = ret.is_discarded();
      } else {
        auto ec = json::read<opts>(ret, queryStrRes);
        if (ec) {
          auto prefixJsonContent = queryStrRes.substr(0, std::min<int>(queryStrRes.size(), 20));
          log::error("Error while reading json content '{}{}': {}", prefixJsonContent,
                     prefixJsonContent.size() < queryStrRes.size() ? "..." : "", json::format_error(ec, queryStrRes));
          parsingError = true;
        } else {
          parsingError = false;
        }
      }

    } while ((parsingError || responseStatus(ret) == Status::kResponseError) &&
             ++nbRetries <= _queryRetryPolicy.nbMaxRetries);

    if (nbRetries > _queryRetryPolicy.nbMaxRetries) {
      switch (_queryRetryPolicy.tooManyFailuresPolicy) {
        case QueryRetryPolicy::TooManyFailuresPolicy::kReturnEmpty:
          if constexpr (std::is_same_v<T, json::container>) {
            log::error("Too many query errors, returning empty json");
            ret = json::container::object();
          } else {
            log::error("Too many query errors, returning value initialized object");
            ret = T();
          }
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