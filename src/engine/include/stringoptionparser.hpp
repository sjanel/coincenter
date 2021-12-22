#pragma once

#include <string_view>
#include <utility>

#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "commandlineoptionsparser.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"

namespace cct {
class StringOptionParser {
 public:
  using MarketExchanges = std::pair<Market, PublicExchangeNames>;
  using MonetaryAmountExchanges = std::pair<MonetaryAmount, PublicExchangeNames>;
  using CurrenciesPrivateExchanges = std::tuple<CurrencyCode, CurrencyCode, PrivateExchangeNames>;
  using CurrencyPrivateExchanges = std::pair<CurrencyCode, PrivateExchangeNames>;
  using MonetaryAmountCurrencyPrivateExchanges = std::tuple<MonetaryAmount, CurrencyCode, PrivateExchangeNames>;
  using MonetaryAmountFromToPrivateExchange = std::tuple<MonetaryAmount, PrivateExchangeName, PrivateExchangeName>;
  using MonetaryAmountFromToPublicExchangeToCurrency = std::tuple<MonetaryAmount, PublicExchangeNames, CurrencyCode>;
  using CurrencyPublicExchanges = std::pair<CurrencyCode, PublicExchangeNames>;
  using CurrenciesPublicExchanges = std::tuple<CurrencyCode, CurrencyCode, PublicExchangeNames>;

  explicit StringOptionParser(std::string_view optFullStr) : _opt(optFullStr) {}

  PublicExchangeNames getExchanges() const;

  PrivateExchangeNames getPrivateExchanges() const;

  MarketExchanges getMarketExchanges() const;

  CurrencyPrivateExchanges getCurrencyPrivateExchanges() const;

  MonetaryAmountExchanges getMonetaryAmountExchanges() const;

  CurrenciesPrivateExchanges getCurrenciesPrivateExchanges(bool currenciesShouldBeSet = true) const;

  MonetaryAmountCurrencyPrivateExchanges getMonetaryAmountCurrencyPrivateExchanges() const;

  MonetaryAmountFromToPrivateExchange getMonetaryAmountFromToPrivateExchange() const;

  CurrencyPublicExchanges getCurrencyPublicExchanges() const;

  CurrenciesPublicExchanges getCurrenciesPublicExchanges() const;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 protected:
  std::size_t getNextCommaPos(std::size_t startPos = 0, bool throwIfNone = true) const;

  string _opt;
};
}  // namespace cct