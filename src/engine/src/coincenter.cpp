#include "coincenter.hpp"

#include <algorithm>
#include <chrono>
#include <execution>
#include <span>

#include "cct_exception.hpp"
#include "cct_smallvector.hpp"
#include "cct_time_helpers.hpp"
#include "cct_variadictable.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"

namespace cct {
using SelectedExchanges = Coincenter::SelectedExchanges;

Coincenter::Coincenter(const CoincenterInfo &coincenterInfo, FiatConverter &fiatConverter,
                       api::CryptowatchAPI &cryptowatchAPI, ExchangeVector &&exchanges)
    : _coincenterInfo(coincenterInfo),
      _fiatConverter(fiatConverter),
      _exchanges(std::move(exchanges)),
      _cryptowatchAPI(cryptowatchAPI) {}

namespace {

Exchange &RetrieveUniqueCandidate(PrivateExchangeName privateExchangeName, std::span<Exchange> exchanges) {
  SelectedExchanges ret;
  for (Exchange &exchange : exchanges) {
    if (privateExchangeName.name() == exchange.name()) {
      if (privateExchangeName.isKeyNameDefined() && exchange.keyName() != privateExchangeName.keyName()) {
        continue;
      }
      ret.push_back(std::addressof(exchange));
    }
  }
  if (ret.empty()) {
    throw exception("Cannot find exchange " + privateExchangeName.str());
  }
  if (ret.size() > 1) {
    throw exception("Several private exchanges found for " + privateExchangeName.str() +
                    " - remove ambiguity by specifying key name");
  }
  return *ret.front();
}
}  // namespace

MarketOrderBooks Coincenter::getMarketOrderBooks(Market m, std::span<const PublicExchangeName> exchangeNames,
                                                 std::optional<int> depth) {
  MarketOrderBooks ret;
  for (Exchange *e : RetrieveSelectedExchanges(exchangeNames, _exchanges)) {
    ret.push_back(depth ? e->apiPublic().queryOrderBook(m, *depth) : e->apiPublic().queryOrderBook(m));
  }
  return ret;
}

Coincenter::MarketOrderBookConversionRates Coincenter::getMarketOrderBooks(
    Market m, std::span<const PublicExchangeName> exchangeNames, CurrencyCode equiCurrencyCode,
    std::optional<int> depth) {
  MarketOrderBookConversionRates ret;
  for (Exchange *e : RetrieveSelectedExchanges(exchangeNames, _exchanges)) {
    std::optional<MonetaryAmount> optConversionRate =
        equiCurrencyCode == CurrencyCode::kNeutral
            ? std::nullopt
            : e->apiPublic().convertAtAveragePrice(MonetaryAmount("1", m.quote()), equiCurrencyCode);
    ret.emplace_back(
        MarketOrderBook(depth ? e->apiPublic().queryOrderBook(m, *depth) : e->apiPublic().queryOrderBook(m)),
        optConversionRate);
  }
  return ret;
}

BalancePortfolio Coincenter::getBalance(std::span<const PrivateExchangeName> privateExchangeNames,
                                        CurrencyCode equiCurrency) {
  BalancePortfolio ret;
  for (Exchange &exchange : _exchanges) {
    const bool computeBalance = (privateExchangeNames.size() == 1 && privateExchangeNames.front().name() == "all" &&
                                 exchange.hasPrivateAPI()) ||
                                std::any_of(privateExchangeNames.begin(), privateExchangeNames.end(),
                                            [&exchange](const PrivateExchangeName &privateExchangeName) {
                                              return exchange.matchesKeyNameWildcard(privateExchangeName);
                                            });
    if (computeBalance) {
      ret.merge(exchange.apiPrivate().queryAccountBalance(equiCurrency));
    }
  }
  return ret;
}

void Coincenter::printBalance(const PrivateExchangeNames &privateExchangeNames, CurrencyCode balanceCurrencyCode) {
  std::string exchangesStr;
  for (const PrivateExchangeName &privateExchangeName : privateExchangeNames) {
    exchangesStr.append(privateExchangeName.str());
  }
  log::info("Query balance from {}", exchangesStr);
  BalancePortfolio portfolio = getBalance(privateExchangeNames, balanceCurrencyCode);
  portfolio.print(std::cout);
}

void Coincenter::printConversionPath(std::span<const PublicExchangeName> exchangeNames, CurrencyCode fromCurrencyCode,
                                     CurrencyCode toCurrencyCode) {
  std::string exchangesStr;
  for (const PublicExchangeName &exchangeName : exchangeNames) {
    exchangesStr.append(exchangeName);
  }
  log::info("Query conversion path from {}", exchangesStr);
  VariadicTable<std::string, std::string> vt({"Exchange", "Fastest conversion path"});
  for (Exchange *e : RetrieveSelectedExchanges(exchangeNames, _exchanges)) {
    std::string conversionPathStr;
    api::ExchangePublic::Currencies conversionPath =
        e->apiPublic().findFastestConversionPath(fromCurrencyCode, toCurrencyCode);
    if (conversionPath.empty()) {
      conversionPathStr = "--- Impossible ---";
    } else {
      for (CurrencyCode currencyCode : conversionPath) {
        if (!conversionPathStr.empty()) {
          conversionPathStr.push_back('-');
        }
        conversionPathStr.append(currencyCode.str());
      }
    }
    vt.addRow(std::string(e->name()), conversionPathStr);
  }
  vt.print();
}

MonetaryAmount Coincenter::trade(MonetaryAmount &startAmount, CurrencyCode toCurrency,
                                 const PrivateExchangeName &privateExchangeName,
                                 const api::TradeOptions &tradeOptions) {
  Exchange &exchange = RetrieveUniqueCandidate(privateExchangeName, _exchanges);
  return exchange.apiPrivate().trade(startAmount, toCurrency, tradeOptions);
}

WithdrawInfo Coincenter::withdraw(MonetaryAmount grossAmount, const PrivateExchangeName &fromPrivateExchangeName,
                                  const PrivateExchangeName &toPrivateExchangeName) {
  Exchange &fromExchange = RetrieveUniqueCandidate(fromPrivateExchangeName, _exchanges);
  Exchange &toExchange = RetrieveUniqueCandidate(toPrivateExchangeName, _exchanges);
  const std::array<Exchange *, 2> exchangePair = {std::addressof(fromExchange), std::addressof(toExchange)};
  cct::vector<CurrencyExchangeFlatSet> currencyExchangeSets(2);
  std::transform(std::execution::par, exchangePair.begin(), exchangePair.end(), currencyExchangeSets.begin(),
                 [](Exchange *e) { return e->queryTradableCurrencies(); });

  const CurrencyCode currencyCode = grossAmount.currencyCode();
  if (!fromExchange.canWithdraw(currencyCode, currencyExchangeSets.front())) {
    throw exception("It's currently not possible to withdraw " + std::string(currencyCode.str()) + " from " +
                    fromPrivateExchangeName.str());
  }
  if (!toExchange.canDeposit(currencyCode, currencyExchangeSets.back())) {
    throw exception("It's currently not possible to deposit " + std::string(currencyCode.str()) + " to " +
                    toPrivateExchangeName.str());
  }

  return fromExchange.apiPrivate().withdraw(grossAmount, toExchange.apiPrivate());
}

PublicExchangeNames Coincenter::getPublicExchangeNames() const {
  PublicExchangeNames ret;
  ret.reserve(_exchanges.size());
  std::transform(std::begin(_exchanges), std::end(_exchanges), std::back_inserter(ret),
                 [](const Exchange &e) { return std::string(e.name()); });
  return ret;
}

void Coincenter::updateFileCaches() const {
  _cryptowatchAPI.updateCacheFile();
  _fiatConverter.updateCacheFile();
  for (const Exchange &exchange : _exchanges) {
    exchange.updateCacheFile();
  }
}

Coincenter::SelectedExchanges Coincenter::RetrieveSelectedExchanges(std::span<const PublicExchangeName> exchangeNames,
                                                                    std::span<Exchange> exchanges) {
  SelectedExchanges ret;
  for (std::string_view exchangeName : exchangeNames) {
    auto exchangeIt = std::find_if(exchanges.begin(), exchanges.end(),
                                   [exchangeName](const Exchange &e) { return e.name() == exchangeName; });
    if (exchangeIt == exchanges.end()) {
      throw exception("Cannot find exchange " + std::string(exchangeName));
    }
    ret.push_back(std::addressof(*exchangeIt));
  }
  return ret;
}

}  // namespace cct
