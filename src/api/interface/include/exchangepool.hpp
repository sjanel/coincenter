#pragma once

#include <span>

#include "binancepublicapi.hpp"
#include "bithumbpublicapi.hpp"
#include "cct_vector.hpp"
#include "exchange.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"
#include "huobipublicapi.hpp"
#include "krakenpublicapi.hpp"
#include "kucoinpublicapi.hpp"
#include "upbitpublicapi.hpp"

namespace cct {

class CoincenterInfo;
class FiatConverter;

namespace api {
class CommonAPI;
class APIKeysProvider;
}  // namespace api

class ExchangePool {
 public:
  ExchangePool(const CoincenterInfo& coincenterInfo, FiatConverter& fiatConverter, api::CommonAPI& commonAPI,
               const api::APIKeysProvider& apiKeyProvider);

  std::span<Exchange> exchanges() { return _exchanges; }
  std::span<const Exchange> exchanges() const { return _exchanges; }

 private:
  using ExchangeVector = vector<Exchange>;

  const CoincenterInfo& _coincenterInfo;
  FiatConverter& _fiatConverter;
  api::CommonAPI& _commonAPI;
  const api::APIKeysProvider& _apiKeyProvider;

  // Public exchanges
  api::BinancePublic _binancePublic;
  api::BithumbPublic _bithumbPublic;
  api::HuobiPublic _huobiPublic;
  api::KrakenPublic _krakenPublic;
  api::KucoinPublic _kucoinPublic;
  api::UpbitPublic _upbitPublic;

  ExchangeVector _exchanges;
};

}  // namespace cct