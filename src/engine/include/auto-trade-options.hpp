#pragma once

#include <map>

#include "auto-trade-config.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "cct_vector.hpp"
#include "exchange-name-enum.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"

namespace cct {

class AutoTradeOptions {
 public:
  using AccountAutoTradeOptionsPtrVector =
      SmallVector<const schema::AutoTradeExchangeConfig *, kTypicalNbPrivateAccounts>;

  struct MarketExchanges {
    Market market;
    ExchangeNames privateExchangeNames;
    const schema::AutoTradeMarketConfig *pMarketAutoTradeOptions{};
  };

  using MarketStatusVector = vector<MarketExchanges>;

  struct MarketExchangeOptions {
    MarketStatusVector marketStatusVector;
  };

  struct PublicExchangeMarketOptions {
    ExchangeName publicExchangeName;
    MarketExchangeOptions marketExchangeOptions;
  };

  using PublicExchangeMarketOptionsVector = FixedCapacityVector<schema::AutoTradeExchangeConfig, kNbSupportedExchanges>;

  AutoTradeOptions() noexcept = default;

  explicit AutoTradeOptions(schema::AutoTradeConfig &&autoTradeConfig);

  auto begin() const { return _autoTradeConfig.begin(); }
  auto end() const { return _autoTradeConfig.end(); }

  ExchangeNames exchangeNames() const;

  ExchangeNameEnumVector publicExchanges() const;

  AccountAutoTradeOptionsPtrVector accountAutoTradeOptionsPtr(std::string_view publicExchangeName) const;

  const schema::AutoTradeExchangeConfig &operator[](ExchangeNameEnum exchangeNameEnum) const;

 private:
  schema::AutoTradeConfig _autoTradeConfig;
};

}  // namespace cct