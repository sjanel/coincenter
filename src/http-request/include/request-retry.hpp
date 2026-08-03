#pragma once

#include <cstdint>
#include <thread>
#include <utility>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_type_traits.hpp"
#include "httpclient.hpp"
#include "httprequestoptions.hpp"
#include "durationstring.hpp"
#include "query-retry-policy.hpp"
#include "timedef.hpp"
#include "unreachable.hpp"
#include "write-json.hpp"

namespace cct {

/// Utility class to factorize basic retry mechanism around httpClient query.
/// Request options remain constant during calls.
class RequestRetry {
 public:
  enum class Status : int8_t { kResponseError, kResponseOK };

 private:
  // {raw_string, error_on_const_read} are opts_ex (derived) members; base members
  // (error_on_unknown_keys, minified) go in the nested brace.
  static constexpr auto kDefaultJsonOpts =
      json::opts_ex{{.error_on_unknown_keys = false, .minified = true}, /*raw_string*/ true,
                    /*error_on_const_read*/ true};

 public:
  RequestRetry(HttpClient &httpClient, HttpRequestOptions requestOptions, QueryRetryPolicy queryRetryPolicy = QueryRetryPolicy())
      : _httpClient(httpClient), _requestOptions(std::move(requestOptions)), _queryRetryPolicy(queryRetryPolicy) {}

  template <class T, auto opts = kDefaultJsonOpts>
  T query(const auto &endpoint, auto responseStatus) {
    return query<T, opts>(endpoint, responseStatus, [](HttpRequestOptions &) {});
  }

  template <class T, auto opts = kDefaultJsonOpts>
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

      postDataUpdateFunc(_requestOptions);

      auto queryStrRes = _httpClient.query(endpoint, _requestOptions);
      auto ec = json::read<opts>(ret, queryStrRes);
      if (ec) {
        auto prefixJsonContent = queryStrRes.substr(0, std::min<int>(queryStrRes.size(), 30));
        log::error("For endpoint {}{} - error while reading json content '{}{}': {}", _httpClient.getNextBaseUrl(),
                   endpoint, prefixJsonContent, prefixJsonContent.size() < queryStrRes.size() ? "..." : "",
                   json::format_error(ec, queryStrRes));
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

  using trivially_relocatable = is_trivially_relocatable<HttpRequestOptions>::type;

 private:
  HttpClient &_httpClient;
  HttpRequestOptions _requestOptions;
  QueryRetryPolicy _queryRetryPolicy;
};

}  // namespace cct