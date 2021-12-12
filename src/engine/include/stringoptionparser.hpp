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
  using CurrencyCodeFromToPrivateExchanges = std::tuple<CurrencyCode, CurrencyCode, PrivateExchangeNames>;
  using MonetaryAmountCurrencyCodePrivateExchanges = std::tuple<MonetaryAmount, CurrencyCode, PrivateExchangeNames>;
  using MonetaryAmountFromToPrivateExchange = std::tuple<MonetaryAmount, PrivateExchangeName, PrivateExchangeName>;
  using MonetaryAmountFromToPublicExchangeToCurrency = std::tuple<MonetaryAmount, PublicExchangeNames, CurrencyCode>;
  using CurrencyCodePublicExchanges = std::pair<CurrencyCode, PublicExchangeNames>;
  using CurrencyPrivateExchanges = std::pair<CurrencyCode, PrivateExchangeNames>;

  explicit StringOptionParser(std::string_view optFullStr) : _opt(optFullStr) {}

  PublicExchangeNames getExchanges() const;

  PrivateExchangeNames getPrivateExchanges() const;

  CurrencyPrivateExchanges getCurrencyPrivateExchanges() const;

  MarketExchanges getMarketExchanges() const;

  MonetaryAmountExchanges getMonetaryAmountExchanges() const;

  CurrencyCodeFromToPrivateExchanges getFromToCurrencyCodePrivateExchanges() const;

  MonetaryAmountCurrencyCodePrivateExchanges getMonetaryAmountCurrencyCodePrivateExchanges() const;

  MonetaryAmountFromToPrivateExchange getMonetaryAmountFromToPrivateExchange() const;

  CurrencyCodePublicExchanges getCurrencyCodePublicExchanges() const;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 protected:
  std::size_t getNextCommaPos(std::size_t startPos = 0, bool throwIfNone = true) const;

  string _opt;
};
}  // namespace cct