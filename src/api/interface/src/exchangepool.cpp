#include "exchangepool.hpp"

#include <memory>
#include <string_view>
#include <utility>

#include "apikey.hpp"
#include "apikeysprovider.hpp"
#include "binanceprivateapi.hpp"
#include "bithumbprivateapi.hpp"
#include "bithumbpublicapi.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"
#include "huobiprivateapi.hpp"
#include "krakenprivateapi.hpp"
#include "kucoinprivateapi.hpp"
#include "upbitprivateapi.hpp"

namespace cct {
ExchangePool::ExchangePool(const CoincenterInfo& coincenterInfo, FiatConverter& fiatConverter,
                           api::CommonAPI& commonAPI, const api::APIKeysProvider& apiKeyProvider)
    : _coincenterInfo(coincenterInfo),
      _fiatConverter(fiatConverter),
      _commonAPI(commonAPI),
      _apiKeyProvider(apiKeyProvider),
      _binancePublic(_coincenterInfo, _fiatConverter, _commonAPI),
      _bithumbPublic(_coincenterInfo, _fiatConverter, _commonAPI),
      _huobiPublic(_coincenterInfo, _fiatConverter, _commonAPI),
      _krakenPublic(_coincenterInfo, _fiatConverter, _commonAPI),
      _kucoinPublic(_coincenterInfo, _fiatConverter, _commonAPI),
      _upbitPublic(_coincenterInfo, _fiatConverter, _commonAPI) {
  for (std::string_view exchangeStr : kSupportedExchanges) {
    api::ExchangePublic* exchangePublic;
    if (exchangeStr == "binance") {
      exchangePublic = &_binancePublic;
    } else if (exchangeStr == "bithumb") {
      exchangePublic = &_bithumbPublic;
    } else if (exchangeStr == "huobi") {
      exchangePublic = &_huobiPublic;
    } else if (exchangeStr == "kraken") {
      exchangePublic = &_krakenPublic;
    } else if (exchangeStr == "kucoin") {
      exchangePublic = &_kucoinPublic;
    } else if (exchangeStr == "upbit") {
      exchangePublic = &_upbitPublic;
    } else {
      throw exception("Should not happen, unsupported platform {}", exchangeStr);
    }

    const bool canUsePrivateExchange = _apiKeyProvider.contains(exchangeStr);
    const ExchangeConfig& exchangeConfig = _coincenterInfo.exchangeConfig(exchangePublic->name());
    if (canUsePrivateExchange) {
      for (std::string_view keyName : _apiKeyProvider.getKeyNames(exchangeStr)) {
        std::unique_ptr<api::ExchangePrivate> exchangePrivate;
        ExchangeName exchangeName(exchangeStr, keyName);
        const api::APIKey& apiKey = _apiKeyProvider.get(exchangeName);
        if (exchangePublic == &_binancePublic) {
          exchangePrivate = std::make_unique<api::BinancePrivate>(_coincenterInfo, _binancePublic, apiKey);
        } else if (exchangePublic == &_bithumbPublic) {
          exchangePrivate = std::make_unique<api::BithumbPrivate>(_coincenterInfo, _bithumbPublic, apiKey);
        } else if (exchangePublic == &_huobiPublic) {
          exchangePrivate = std::make_unique<api::HuobiPrivate>(_coincenterInfo, _huobiPublic, apiKey);
        } else if (exchangePublic == &_krakenPublic) {
          exchangePrivate = std::make_unique<api::KrakenPrivate>(_coincenterInfo, _krakenPublic, apiKey);
        } else if (exchangePublic == &_kucoinPublic) {
          exchangePrivate = std::make_unique<api::KucoinPrivate>(_coincenterInfo, _kucoinPublic, apiKey);
        } else if (exchangePublic == &_upbitPublic) {
          exchangePrivate = std::make_unique<api::UpbitPrivate>(_coincenterInfo, _upbitPublic, apiKey);
        } else {
          throw exception("Unsupported platform {}", exchangeStr);
        }

        if (exchangeConfig.shouldValidateApiKey()) {
          if (exchangePrivate->validateApiKey()) {
            log::info("{} api key is valid", exchangeName);
          } else {
            log::error("{} api key is invalid, do not consider it", exchangeName);
            continue;
          }
        }

        _exchanges.emplace_back(exchangeConfig, *exchangePublic, std::move(exchangePrivate));
      }
    } else {
      _exchanges.emplace_back(exchangeConfig, *exchangePublic);
    }
  }
  _exchanges.shrink_to_fit();
}

}  // namespace cct
