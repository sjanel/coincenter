#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <utility>

#include "cct_string.hpp"
#include "cct_vector.hpp"
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
  using CurrencyFromToPrivateExchange = std::pair<CurrencyCode, ExchangeNames>;
  using MonetaryAmountFromToPrivateExchange = std::tuple<MonetaryAmount, bool, ExchangeNames>;
  using MonetaryAmountFromToPublicExchangeToCurrency = std::tuple<MonetaryAmount, ExchangeNames, CurrencyCode>;
  using CurrencyPublicExchanges = std::pair<CurrencyCode, ExchangeNames>;
  using CurrenciesPublicExchanges = std::tuple<CurrencyCode, CurrencyCode, ExchangeNames>;

  enum class CurrencyIs : int8_t { kMandatory, kOptional };

  explicit StringOptionParser(std::string_view optFullStr) : _opt(optFullStr) {}

  ExchangeNames getExchanges() const;

  MarketExchanges getMarketExchanges() const;

  CurrencyPrivateExchanges getCurrencyPrivateExchanges(CurrencyIs currencyIs) const;

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

  vector<string> getCSVValues() const;

 protected:
  std::size_t getNextCommaPos(std::size_t startPos = 0, bool throwIfNone = true) const;

  MonetaryAmountCurrencyPrivateExchanges getMonetaryAmountCurrencyPrivateExchanges(bool withCurrency) const;

  std::string_view _opt;
};
}  // namespace cct