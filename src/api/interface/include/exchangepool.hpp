#pragma once

#include <forward_list>
#include <span>
#include <string_view>

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
class BinancePrivate;
class BithumbPrivate;
class HuobiPrivate;
class KrakenPrivate;
class KucoinPrivate;
class UpbitPrivate;

class CryptowatchAPI;
class APIKeysProvider;
}  // namespace api

class ExchangePool {
 public:
  ExchangePool(const CoincenterInfo& coincenterInfo, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI,
               const api::APIKeysProvider& apiKeyProvider);

  ExchangePool(const ExchangePool&) = delete;
  ExchangePool(ExchangePool&&) = delete;
  ExchangePool& operator=(const ExchangePool&) = delete;
  ExchangePool& operator=(ExchangePool&&) = delete;

  ~ExchangePool();

  std::span<Exchange> exchanges() { return _exchanges; }
  std::span<const Exchange> exchanges() const { return _exchanges; }

 private:
  using ExchangeVector = vector<Exchange>;

  const CoincenterInfo& _coincenterInfo;
  FiatConverter& _fiatConverter;
  api::CryptowatchAPI& _cryptowatchAPI;
  const api::APIKeysProvider& _apiKeyProvider;

  // Public exchanges
  api::BinancePublic _binancePublic;
  api::BithumbPublic _bithumbPublic;
  api::HuobiPublic _huobiPublic;
  api::KrakenPublic _krakenPublic;
  api::KucoinPublic _kucoinPublic;
  api::UpbitPublic _upbitPublic;

  // Private exchanges (based on provided keys)
  // Use std::forward_list to guarantee validity of the iterators and pointers, as we give them to Exchange object as
  // pointers
  std::forward_list<api::BinancePrivate> _binancePrivates;
  std::forward_list<api::BithumbPrivate> _bithumbPrivates;
  std::forward_list<api::HuobiPrivate> _huobiPrivates;
  std::forward_list<api::KrakenPrivate> _krakenPrivates;
  std::forward_list<api::KucoinPrivate> _kucoinPrivates;
  std::forward_list<api::UpbitPrivate> _upbitPrivates;

  ExchangeVector _exchanges;
};

}  // namespace cct