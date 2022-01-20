#pragma once

#include <string_view>
#include <utility>

#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
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
  using MonetaryAmountCurrencyPrivateExchanges = std::tuple<MonetaryAmount, bool, CurrencyCode, PrivateExchangeNames>;
  using MonetaryAmountFromToPrivateExchange = std::tuple<MonetaryAmount, PrivateExchangeName, PrivateExchangeName>;
  using MonetaryAmountFromToPublicExchangeToCurrency = std::tuple<MonetaryAmount, PublicExchangeNames, CurrencyCode>;
  using CurrencyPublicExchanges = std::pair<CurrencyCode, PublicExchangeNames>;
  using CurrenciesPublicExchanges = std::tuple<CurrencyCode, CurrencyCode, PublicExchangeNames>;

  explicit StringOptionParser(std::string_view optFullStr) : _opt(optFullStr) {}

  PublicExchangeNames getExchanges() const;

  MarketExchanges getMarketExchanges() const;

  CurrencyPrivateExchanges getCurrencyPrivateExchanges() const;

  MonetaryAmountExchanges getMonetaryAmountExchanges() const;

  CurrenciesPrivateExchanges getCurrenciesPrivateExchanges(bool currenciesShouldBeSet = true) const;

  MonetaryAmountCurrencyPrivateExchanges getMonetaryAmountCurrencyPrivateExchanges() const;

  MonetaryAmountFromToPrivateExchange getMonetaryAmountFromToPrivateExchange() const;

  CurrencyPublicExchanges getCurrencyPublicExchanges() const;

  CurrenciesPublicExchanges getCurrenciesPublicExchanges() const;

  vector<std::string_view> getCSVValues() const;

 protected:
  std::size_t getNextCommaPos(std::size_t startPos = 0, bool throwIfNone = true) const;

  std::string_view _opt;
};
}  // namespace cct