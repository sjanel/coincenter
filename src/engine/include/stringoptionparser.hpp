#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "commandlineoptionsparser.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"

namespace cct {
class AnyParser {
 public:
  using MarketExchanges = std::pair<Market, PublicExchangeNames>;
  using MonetaryAmountExchanges = std::pair<MonetaryAmount, PublicExchangeNames>;
  using MonetaryAmountCurrencyCodePrivateExchange = std::tuple<MonetaryAmount, CurrencyCode, PrivateExchangeName>;
  using MonetaryAmountFromToPrivateExchange = std::tuple<MonetaryAmount, PrivateExchangeName, PrivateExchangeName>;
  using MonetaryAmountFromToPublicExchangeToCurrency = std::tuple<MonetaryAmount, PublicExchangeNames, CurrencyCode>;

  explicit AnyParser(std::string_view optFullStr) : _opt(optFullStr) {}

  PublicExchangeNames getExchanges() const;

  PrivateExchangeNames getPrivateExchanges() const;

  MarketExchanges getMarketExchanges() const;

  MonetaryAmountExchanges getMonetaryAmountExchanges() const;

  MonetaryAmountCurrencyCodePrivateExchange getMonetaryAmountCurrencyCodePrivateExchange() const;

  MonetaryAmountFromToPrivateExchange getMonetaryAmountFromToPrivateExchange() const;

  MonetaryAmountFromToPublicExchangeToCurrency getMonetaryAmountFromToPublicExchangeToCurrency() const;

 private:
  std::size_t getNextCommaPos(std::size_t startPos = 0, bool throwIfNone = true) const;
  std::string _opt;
};
}  // namespace cct