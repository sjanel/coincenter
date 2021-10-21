#pragma once

#include <string_view>
#include <utility>

#include "cct_string.hpp"
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
  using MonetaryAmountCurrencyCodePrivateExchange = std::tuple<MonetaryAmount, CurrencyCode, PrivateExchangeName>;
  using MonetaryAmountFromToPrivateExchange = std::tuple<MonetaryAmount, PrivateExchangeName, PrivateExchangeName>;
  using MonetaryAmountFromToPublicExchangeToCurrency = std::tuple<MonetaryAmount, PublicExchangeNames, CurrencyCode>;
  using CurrencyCodePublicExchanges = std::pair<CurrencyCode, PublicExchangeNames>;

  explicit StringOptionParser(std::string_view optFullStr) : _opt(optFullStr) {}

  PublicExchangeNames getExchanges() const;

  PrivateExchangeNames getPrivateExchanges() const;

  MarketExchanges getMarketExchanges() const;

  MonetaryAmountExchanges getMonetaryAmountExchanges() const;

  MonetaryAmountCurrencyCodePrivateExchange getMonetaryAmountCurrencyCodePrivateExchange() const;

  MonetaryAmountFromToPrivateExchange getMonetaryAmountFromToPrivateExchange() const;

  CurrencyCodePublicExchanges getCurrencyCodePublicExchanges() const;

 protected:
  std::size_t getNextCommaPos(std::size_t startPos = 0, bool throwIfNone = true) const;

  string _opt;
};
}  // namespace cct