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
  using MarketExchanges = std::pair<Market, ExchangeNames>;
  using CurrenciesPrivateExchanges = std::tuple<CurrencyCode, CurrencyCode, ExchangeNames>;
  using CurrencyPrivateExchanges = std::pair<CurrencyCode, ExchangeNames>;
  using MonetaryAmountCurrencyPrivateExchanges = std::tuple<MonetaryAmount, bool, CurrencyCode, ExchangeNames>;
  using CurrencyFromToPrivateExchange = std::tuple<CurrencyCode, ExchangeName, ExchangeName>;
  using MonetaryAmountFromToPrivateExchange = std::tuple<MonetaryAmount, bool, ExchangeName, ExchangeName>;
  using MonetaryAmountFromToPublicExchangeToCurrency = std::tuple<MonetaryAmount, ExchangeNames, CurrencyCode>;
  using CurrencyPublicExchanges = std::pair<CurrencyCode, ExchangeNames>;
  using CurrenciesPublicExchanges = std::tuple<CurrencyCode, CurrencyCode, ExchangeNames>;

  explicit StringOptionParser(std::string_view optFullStr) : _opt(optFullStr) {}

  ExchangeNames getExchanges() const;

  MarketExchanges getMarketExchanges() const;

  CurrencyPrivateExchanges getCurrencyPrivateExchanges() const;

  auto getMonetaryAmountPrivateExchanges() const {
    auto ret = getMonetaryAmountCurrencyPrivateExchanges(false);
    return std::make_tuple(std::move(std::get<0>(ret)), std::move(std::get<1>(ret)), std::move(std::get<3>(ret)));
  }

  CurrenciesPrivateExchanges getCurrenciesPrivateExchanges(bool currenciesShouldBeSet = true) const;

  MonetaryAmountCurrencyPrivateExchanges getMonetaryAmountCurrencyPrivateExchanges() const {
    return getMonetaryAmountCurrencyPrivateExchanges(true);
  }

  CurrencyFromToPrivateExchange getCurrencyFromToPrivateExchange() const;

  MonetaryAmountFromToPrivateExchange getMonetaryAmountFromToPrivateExchange() const;

  CurrencyPublicExchanges getCurrencyPublicExchanges() const;

  CurrenciesPublicExchanges getCurrenciesPublicExchanges() const;

  vector<std::string_view> getCSVValues() const;

 protected:
  std::size_t getNextCommaPos(std::size_t startPos = 0, bool throwIfNone = true) const;

  MonetaryAmountCurrencyPrivateExchanges getMonetaryAmountCurrencyPrivateExchanges(bool withCurrency) const;

  std::string_view _opt;
};
}  // namespace cct