#include "exchangepool.hpp"

#include <memory>

#include "apikeysprovider.hpp"
#include "binanceprivateapi.hpp"
#include "bithumbprivateapi.hpp"
#include "cct_exception.hpp"
#include "cct_string.hpp"
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
  for (std::string_view exchangeName : kSupportedExchanges) {
    api::ExchangePublic* exchangePublic;
    if (exchangeName == "binance") {
      exchangePublic = std::addressof(_binancePublic);
    } else if (exchangeName == "bithumb") {
      exchangePublic = std::addressof(_bithumbPublic);
    } else if (exchangeName == "huobi") {
      exchangePublic = std::addressof(_huobiPublic);
    } else if (exchangeName == "kraken") {
      exchangePublic = std::addressof(_krakenPublic);
    } else if (exchangeName == "kucoin") {
      exchangePublic = std::addressof(_kucoinPublic);
    } else if (exchangeName == "upbit") {
      exchangePublic = std::addressof(_upbitPublic);
    } else {
      throw exception("Should not happen, unsupported platform {}", exchangeName);
    }

    const bool canUsePrivateExchange = _apiKeyProvider.contains(exchangeName);
    if (canUsePrivateExchange) {
      for (std::string_view keyName : _apiKeyProvider.getKeyNames(exchangeName)) {
        api::ExchangePrivate* exchangePrivate;
        const api::APIKey& apiKey = _apiKeyProvider.get(ExchangeName(exchangeName, keyName));
        if (exchangeName == "binance") {
          exchangePrivate = std::addressof(_binancePrivates.emplace_front(_coincenterInfo, _binancePublic, apiKey));
        } else if (exchangeName == "bithumb") {
          exchangePrivate = std::addressof(_bithumbPrivates.emplace_front(_coincenterInfo, _bithumbPublic, apiKey));
        } else if (exchangeName == "huobi") {
          exchangePrivate = std::addressof(_huobiPrivates.emplace_front(_coincenterInfo, _huobiPublic, apiKey));
        } else if (exchangeName == "kraken") {
          exchangePrivate = std::addressof(_krakenPrivates.emplace_front(_coincenterInfo, _krakenPublic, apiKey));
        } else if (exchangeName == "kucoin") {
          exchangePrivate = std::addressof(_kucoinPrivates.emplace_front(_coincenterInfo, _kucoinPublic, apiKey));
        } else if (exchangeName == "upbit") {
          exchangePrivate = std::addressof(_upbitPrivates.emplace_front(_coincenterInfo, _upbitPublic, apiKey));
        } else {
          throw exception("Should not happen, unsupported platform {}", exchangeName);
        }

        _exchanges.emplace_back(_coincenterInfo.exchangeInfo(exchangePublic->name()), *exchangePublic,
                                *exchangePrivate);
      }
    } else {
      _exchanges.emplace_back(_coincenterInfo.exchangeInfo(exchangePublic->name()), *exchangePublic);
    }
  }
  _exchanges.shrink_to_fit();
}

ExchangePool::~ExchangePool() = default;

}  // namespace cct