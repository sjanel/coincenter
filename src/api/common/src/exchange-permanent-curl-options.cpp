#include "exchange-permanent-curl-options.hpp"

#include "exchangeconfig.hpp"

namespace cct::api {

ExchangePermanentCurlOptions::ExchangePermanentCurlOptions(const ExchangeConfig &exchangeConfig)
    : _exchangeConfig(exchangeConfig) {}

PermanentCurlOptions::Builder ExchangePermanentCurlOptions::builderBase(Api api) const {
  PermanentCurlOptions::Builder builder;

  builder.setAcceptedEncoding(_exchangeConfig.acceptEncoding())
      .setRequestCallLogLevel(_exchangeConfig.requestsCallLogLevel())
      .setRequestAnswerLogLevel(_exchangeConfig.requestsAnswerLogLevel())
      .setTimeout(_exchangeConfig.httpConfig().timeout());

  switch (api) {
    case Api::Private:
      builder.setMinDurationBetweenQueries(_exchangeConfig.privateAPIRate());
      break;
    case Api::Public:
      builder.setMinDurationBetweenQueries(_exchangeConfig.publicAPIRate())
          .setTooManyErrorsPolicy(PermanentCurlOptions::TooManyErrorsPolicy::kReturnEmptyResponse);
      break;
    default:
      break;
  }

  return builder;
}

}  // namespace cct::api