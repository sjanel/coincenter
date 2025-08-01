#pragma once

#include <array>
#include <utility>

#include "cct_fixedcapacityvector.hpp"
#include "exchange-asset-config.hpp"
#include "exchange-general-config.hpp"
#include "exchange-name-enum.hpp"
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
  FixedCapacityVector<std::pair<ExchangeNameEnum, T>, kNbSupportedExchanges> exchange;
};

struct AllExchangeConfigsOptional {
  ExchangeConfigPart<ExchangeGeneralConfigOptional> general;
  ExchangeConfigPart<ExchangeAssetConfig> asset;
  ExchangeConfigPart<ExchangeQueryConfigOptional> query;
  ExchangeConfigPart<ExchangeTradeFeesConfigOptional> tradeFees;
  ExchangeConfigPart<ExchangeWithdrawConfigOptional> withdraw;
};

}  // namespace details

struct ExchangeConfig {
  ExchangeGeneralConfig general;
  ExchangeAssetConfig asset;
  ExchangeQueryConfig query;
  ExchangeTradeFeesConfig tradeFees;
  ExchangeWithdrawConfig withdraw;
};

}  // namespace schema

class AllExchangeConfigs {
 public:
  AllExchangeConfigs() = default;

  explicit AllExchangeConfigs(const LoadConfiguration &loadConfiguration);

  const schema::ExchangeConfig &operator[](ExchangeNameEnum exchangeNameEnum) const {
    return _exchangeConfigs[static_cast<int>(exchangeNameEnum)];
  }

  void mergeWith(schema::details::AllExchangeConfigsOptional &other);

 private:
  std::array<schema::ExchangeConfig, kNbSupportedExchanges> _exchangeConfigs;
};

}  // namespace cct

template <class T>
struct glz::meta<::cct::schema::details::ExchangeConfigPart<T>> {
  using V = ::cct::schema::details::ExchangeConfigPart<T>;
  static constexpr auto value = object("default", &V::def, "exchange", &V::exchange);
};
