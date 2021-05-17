#include "coincenter.hpp"

#include <algorithm>
#include <chrono>
#include <execution>
#include <span>

#include "cct_exception.hpp"
#include "cct_smallvector.hpp"
#include "cct_time_helpers.hpp"
#include "cct_variadictable.hpp"
#include "coincenteroptions.hpp"
#include "coincenterparsedoptions.hpp"
#include "stringoptionparser.hpp"

namespace cct {
using SelectedExchanges = Coincenter::SelectedExchanges;

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

Coincenter::Coincenter(settings::RunMode runMode)
    : _coincenterInfo(runMode),
      _cryptowatchAPI(runMode),
      _apiKeyProvider(runMode),
      _krakenPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _bithumbPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _binancePublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _upbitPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI) {
  for (std::string_view exchangeName : api::ExchangePublic::kSupportedExchanges) {
    api::ExchangePublic *exchangePublic;
    if (exchangeName == "kraken") {
      exchangePublic = std::addressof(_krakenPublic);
    } else if (exchangeName == "binance") {
      exchangePublic = std::addressof(_binancePublic);
    } else if (exchangeName == "bithumb") {
      exchangePublic = std::addressof(_bithumbPublic);
    } else if (exchangeName == "upbit") {
      exchangePublic = std::addressof(_upbitPublic);
    } else {
      throw exception("Should not happen, unsupported platform " + std::string(exchangeName));
    }

    const bool canUsePrivateExchange = _apiKeyProvider.contains(exchangeName);
    if (canUsePrivateExchange) {
      for (const std::string &keyName : _apiKeyProvider.getKeyNames(exchangeName)) {
        api::ExchangePrivate *exchangePrivate;
        const api::APIKey &apiKey = _apiKeyProvider.get(PrivateExchangeName(exchangeName, keyName));
        if (exchangeName == "kraken") {
          exchangePrivate = std::addressof(_krakenPrivates.emplace_front(_coincenterInfo, _krakenPublic, apiKey));
        } else if (exchangeName == "binance") {
          exchangePrivate = std::addressof(_binancePrivates.emplace_front(_coincenterInfo, _binancePublic, apiKey));
        } else if (exchangeName == "bithumb") {
          exchangePrivate = std::addressof(_bithumbPrivates.emplace_front(_coincenterInfo, _bithumbPublic, apiKey));
        } else if (exchangeName == "upbit") {
          exchangePrivate = std::addressof(_upbitPrivates.emplace_front(_coincenterInfo, _upbitPublic, apiKey));
        } else {
          throw exception("Should not happen, unsupported platform " + std::string(exchangeName));
        }

        _exchanges.emplace_back(_coincenterInfo.exchangeInfo(exchangePublic->name()), *exchangePublic,
                                *exchangePrivate);
      }
    } else {
      _exchanges.emplace_back(_coincenterInfo.exchangeInfo(exchangePublic->name()), *exchangePublic);
    }
  }
}

void Coincenter::process(const CoincenterParsedOptions &opts) {
  Coincenter coincenter;

  if (!opts.orderBookExchanges.empty()) {
    std::optional<int> depth;
    if (opts.orderbookDepth != 0) {
      depth = opts.orderbookDepth;
    }
    Coincenter::MarketOrderBookConversionRates marketOrderBooksConversionRates =
        coincenter.getMarketOrderBooks(opts.marketForOrderBook, opts.orderBookExchanges, opts.orderbookCur, depth);
    int orderBookPos = 0;
    for (std::string_view exchangeName : opts.orderBookExchanges) {
      const auto &[marketOrderBook, optConversionRate] = marketOrderBooksConversionRates[orderBookPos];
      log::info("Order book of {} on {} requested{}{}", opts.marketForOrderBook.str(), exchangeName,
                optConversionRate ? " with conversion rate " : "", optConversionRate ? optConversionRate->str() : "");

      if (optConversionRate) {
        marketOrderBook.print(std::cout, *optConversionRate);
      } else {
        if (opts.orderbookCur != CurrencyCode::kNeutral) {
          log::warn("Unable to convert {} into {} on {}", opts.marketForOrderBook.quote().str(),
                    opts.orderbookCur.str(), exchangeName);
        }
        marketOrderBook.print(std::cout);
      }

      ++orderBookPos;
    }
  }

  if (!opts.conversionPathExchanges.empty()) {
    coincenter.printConversionPath(opts.conversionPathExchanges, opts.marketForConversionPath.base(),
                                   opts.marketForConversionPath.quote());
  }

  if (!opts.balancePrivateExchanges.empty()) {
    coincenter.printBalance(opts.balancePrivateExchanges, opts.balanceCurrencyCode);
  }

  if (!opts.startTradeAmount.isZero()) {
    log::warn("Trade {} into {} on {} requested", opts.startTradeAmount.str(), opts.toTradeCurrency.str(),
              opts.tradePrivateExchangeName.str());
    log::warn(opts.tradeOptions.str());
    MonetaryAmount startAmount = opts.startTradeAmount;
    MonetaryAmount toAmount =
        coincenter.trade(startAmount, opts.toTradeCurrency, opts.tradePrivateExchangeName, opts.tradeOptions);
    log::warn("**** Traded {} into {} ****", (opts.startTradeAmount - startAmount).str(), toAmount.str());
  }

  if (!opts.amountToWithdraw.isZero()) {
    log::warn("Withdraw gross {} from {} to {} requested", opts.amountToWithdraw.str(),
              opts.withdrawFromExchangeName.str(), opts.withdrawToExchangeName.str());
    coincenter.withdraw(opts.amountToWithdraw, opts.withdrawFromExchangeName, opts.withdrawToExchangeName);
  }

  coincenter.updateFileCaches();
}

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
