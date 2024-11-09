#include "exchange-permanent-curl-options.hpp"

#include "exchange-query-config.hpp"
#include "permanentcurloptions.hpp"

namespace cct::api {

ExchangePermanentCurlOptions::ExchangePermanentCurlOptions(const schema::ExchangeQueryConfig &queryConfig)
    : _queryConfig(queryConfig) {}

PermanentCurlOptions::Builder ExchangePermanentCurlOptions::builderBase(Api api) const {
  PermanentCurlOptions::Builder builder;

  builder.setAcceptedEncoding(_queryConfig.acceptEncoding)
      .setRequestCallLogLevel(_queryConfig.logLevels.requestsCall)
      .setRequestAnswerLogLevel(_queryConfig.logLevels.requestsAnswer)
      .setTimeout(_queryConfig.http.timeout.duration);

  switch (api) {
    case Api::Private:
      builder.setMinDurationBetweenQueries(_queryConfig.privateAPIRate.duration);
      break;
    case Api::Public:
      builder.setMinDurationBetweenQueries(_queryConfig.publicAPIRate.duration)
          .setTooManyErrorsPolicy(PermanentCurlOptions::TooManyErrorsPolicy::kReturnEmptyResponse);
      break;
    default:
      break;
  }

  return builder;
}

}  // namespace cct::api