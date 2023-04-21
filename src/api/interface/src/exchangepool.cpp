#include "exchangepool.hpp"

#include <memory>

#include "apikeysprovider.hpp"
#include "binanceprivateapi.hpp"
#include "bithumbprivateapi.hpp"
#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "huobiprivateapi.hpp"
#include "krakenprivateapi.hpp"
#include "kucoinprivateapi.hpp"
#include "upbitprivateapi.hpp"

namespace cct {
ExchangePool::ExchangePool(const CoincenterInfo& coincenterInfo, FiatConverter& fiatConverter,
                           api::CryptowatchAPI& cryptowatchAPI, const api::APIKeysProvider& apiKeyProvider)
    : _coincenterInfo(coincenterInfo),
      _fiatConverter(fiatConverter),
      _cryptowatchAPI(cryptowatchAPI),
      _apiKeyProvider(apiKeyProvider),
      _binancePublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _bithumbPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _huobiPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _krakenPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _kucoinPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _upbitPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI) {
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
    const ExchangeInfo& exchangeInfo = _coincenterInfo.exchangeInfo(exchangePublic->name());
    if (canUsePrivateExchange) {
      for (std::string_view keyName : _apiKeyProvider.getKeyNames(exchangeStr)) {
        api::ExchangePrivate* exchangePrivate;
        ExchangeName exchangeName(exchangeStr, keyName);
        const api::APIKey& apiKey = _apiKeyProvider.get(exchangeName);
        if (exchangePublic == &_binancePublic) {
          exchangePrivate = &_binancePrivates.emplace_front(_coincenterInfo, _binancePublic, apiKey);
        } else if (exchangePublic == &_bithumbPublic) {
          exchangePrivate = &_bithumbPrivates.emplace_front(_coincenterInfo, _bithumbPublic, apiKey);
        } else if (exchangePublic == &_huobiPublic) {
          exchangePrivate = &_huobiPrivates.emplace_front(_coincenterInfo, _huobiPublic, apiKey);
        } else if (exchangePublic == &_krakenPublic) {
          exchangePrivate = &_krakenPrivates.emplace_front(_coincenterInfo, _krakenPublic, apiKey);
        } else if (exchangePublic == &_kucoinPublic) {
          exchangePrivate = &_kucoinPrivates.emplace_front(_coincenterInfo, _kucoinPublic, apiKey);
        } else if (exchangePublic == &_upbitPublic) {
          exchangePrivate = &_upbitPrivates.emplace_front(_coincenterInfo, _upbitPublic, apiKey);
        } else {
          throw exception("Unsupported platform {}", exchangeStr);
        }

        if (exchangeInfo.shouldValidateApiKey()) {
          if (exchangePrivate->validateApiKey()) {
            log::info("{} api key is valid", exchangeName);
          } else {
            log::error("{} api key is invalid, do not consider it", exchangeName);
            continue;
          }
        }

        _exchanges.emplace_back(exchangeInfo, *exchangePublic, *exchangePrivate);
      }
    } else {
      _exchanges.emplace_back(exchangeInfo, *exchangePublic);
    }
  }
  _exchanges.shrink_to_fit();
}

ExchangePool::~ExchangePool() = default;

}  // namespace cct
