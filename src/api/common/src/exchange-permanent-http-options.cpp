#include "exchange-permanent-http-options.hpp"

#include "exchange-query-config.hpp"
#include "permanentrequestoptions.hpp"

namespace cct::api {

ExchangePermanentHttpOptions::ExchangePermanentHttpOptions(const schema::ExchangeQueryConfig &queryConfig)
    : _queryConfig(queryConfig) {}

PermanentRequestOptions::Builder ExchangePermanentHttpOptions::builderBase(Api api) const {
  PermanentRequestOptions::Builder builder;

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
          .setTooManyErrorsPolicy(PermanentRequestOptions::TooManyErrorsPolicy::kReturnEmptyResponse);
      break;
    default:
      break;
  }

  return builder;
}

}  // namespace cct::api