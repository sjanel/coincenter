#pragma once

#include <map>

#include "cct_fixedcapacityvector.hpp"
#include "cct_json.hpp"
#include "cct_smallvector.hpp"
#include "cct_vector.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "public-exchange-auto-trade-options.hpp"

namespace cct {

class AutoTradeOptions {
 public:
  using AccountAutoTradeOptionsPtrVector =
      SmallVector<const PublicExchangeAutoTradeOptions *, kTypicalNbPrivateAccounts>;

  struct MarketExchanges {
    Market market;
    ExchangeNames privateExchangeNames;
    const MarketAutoTradeOptions *pMarketAutoTradeOptions{};
  };

  using MarketStatusVector = vector<MarketExchanges>;

  struct MarketExchangeOptions {
    MarketStatusVector marketStatusVector;
  };

  struct PublicExchangeMarketOptions {
    ExchangeName publicExchangeName;
    MarketExchangeOptions marketExchangeOptions;
  };

  using PublicExchangeMarketOptionsVector = FixedCapacityVector<PublicExchangeMarketOptions, kNbSupportedExchanges>;

  AutoTradeOptions() noexcept = default;

  explicit AutoTradeOptions(const json &data);

  auto begin() const { return _options.begin(); }
  auto end() const { return _options.end(); }

  ExchangeNames exchangeNames() const;

  PublicExchangeNameVector publicExchanges() const;

  AccountAutoTradeOptionsPtrVector accountAutoTradeOptionsPtr(std::string_view publicExchangeName) const;

  const PublicExchangeAutoTradeOptions &operator[](const ExchangeName &exchangeName) const;

 private:
  std::map<ExchangeName, PublicExchangeAutoTradeOptions> _options;
};

}  // namespace cct