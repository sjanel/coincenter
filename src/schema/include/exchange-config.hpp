#pragma once

#include <array>
#include <map>

#include "cct_const.hpp"
#include "cct_string.hpp"
#include "exchange-asset-config.hpp"
#include "exchange-query-config.hpp"
#include "exchange-tradefees-config.hpp"
#include "exchange-withdraw-config.hpp"

namespace cct {

class LoadConfiguration;

namespace schema {

namespace details {

template <class T>
struct ExchangeConfigPart {
  T def;  // default is a reserved keyword - we override the json field name below
  std::map<ExchangeNameEnum, T> exchange;
};

struct AllExchangeConfigsOptional {
  ExchangeConfigPart<ExchangeAssetConfig> asset;
  ExchangeConfigPart<ExchangeQueryConfigOptional> query;
  ExchangeConfigPart<ExchangeTradeFeesConfigOptional> tradeFees;
  ExchangeConfigPart<ExchangeWithdrawConfigOptional> withdraw;
};

}  // namespace details

struct ExchangeConfig {
  ExchangeAssetConfig asset;
  ExchangeQueryConfig query;
  ExchangeTradeFeesConfig tradeFees;
  ExchangeWithdrawConfig withdraw;
};

class AllExchangeConfigs {
 public:
  AllExchangeConfigs() = default;

  explicit AllExchangeConfigs(const LoadConfiguration &loadConfiguration);

  const ExchangeConfig &operator[](ExchangeNameEnum exchangeName) const {
    return _exchangeConfigs[static_cast<int>(exchangeName)];
  }

  void mergeWith(const details::AllExchangeConfigsOptional &other);

 private:
  std::array<ExchangeConfig, kNbSupportedExchanges> _exchangeConfigs;
};

}  // namespace schema

}  // namespace cct

template <class T>
struct glz::meta<::cct::schema::details::ExchangeConfigPart<T>> {
  using V = ::cct::schema::details::ExchangeConfigPart<T>;
  static constexpr auto value = object("default", &V::def, "exchange", &V::exchange);
};
