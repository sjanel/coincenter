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
  for (int exchangePos = 0; exchangePos < kNbSupportedExchanges; ++exchangePos) {
    ExchangeNameEnum exchangeNameEnum = static_cast<ExchangeNameEnum>(exchangePos);
    api::ExchangePublic* exchangePublic;
    if (exchangeNameEnum == ExchangeNameEnum::binance) {
      exchangePublic = &_binancePublic;
    } else if (exchangeNameEnum == ExchangeNameEnum::bithumb) {
      exchangePublic = &_bithumbPublic;
    } else if (exchangeNameEnum == ExchangeNameEnum::huobi) {
      exchangePublic = &_huobiPublic;
    } else if (exchangeNameEnum == ExchangeNameEnum::kraken) {
      exchangePublic = &_krakenPublic;
    } else if (exchangeNameEnum == ExchangeNameEnum::kucoin) {
      exchangePublic = &_kucoinPublic;
    } else if (exchangeNameEnum == ExchangeNameEnum::upbit) {
      exchangePublic = &_upbitPublic;
    } else {
      throw exception("Should not happen, unsupported exchange pos {}", exchangePos);
    }

    const auto& exchangeConfig = _coincenterInfo.exchangeConfig(exchangeNameEnum);

    if (!exchangeConfig.general.enabled) {
      continue;
    }

    const bool canUsePrivateExchange = _apiKeyProvider.hasAtLeastOneKey(exchangeNameEnum);
    if (canUsePrivateExchange) {
      for (std::string_view keyName : _apiKeyProvider.getKeyNames(exchangeNameEnum)) {
        std::unique_ptr<api::ExchangePrivate> exchangePrivate;
        ExchangeName exchangeName(exchangeNameEnum, keyName);
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
          throw exception("Should not happen");
        }

        if (exchangeConfig.query.validateApiKey) {
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
  if (_exchanges.empty()) {
    log::error("No exchange enabled, check your configuration");
  } else {
    _exchanges.shrink_to_fit();
  }
}

}  // namespace cct
