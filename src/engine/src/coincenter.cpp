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

std::string_view ToString(const PublicExchangeName &exchangeName) { return exchangeName; }
std::string_view ToString(const PrivateExchangeName &exchangeName) { return exchangeName.str(); }

Coincenter::SelectedExchanges RetrieveSelectedExchanges(std::span<const PublicExchangeName> exchangeNames,
                                                        std::span<Exchange> exchanges) {
  SelectedExchanges ret;
  if (exchangeNames.empty()) {
    std::transform(exchanges.begin(), exchanges.end(), std::back_inserter(ret),
                   [](Exchange &e) { return std::addressof(e); });
  } else {
    for (std::string_view exchangeName : exchangeNames) {
      auto exchangeIt = std::find_if(exchanges.begin(), exchanges.end(),
                                     [exchangeName](const Exchange &e) { return e.name() == exchangeName; });
      if (exchangeIt == exchanges.end()) {
        throw exception("Cannot find exchange " + std::string(exchangeName));
      }
      ret.push_back(std::addressof(*exchangeIt));
    }
  }

  return ret;
}

using UniquePublicExchanges = cct::FlatSet<api::ExchangePublic *>;

UniquePublicExchanges RetrieveUniquePublicExchanges(std::span<const PublicExchangeName> exchangeNames,
                                                    std::span<Exchange> exchanges) {
  SelectedExchanges selectedExchanges = RetrieveSelectedExchanges(exchangeNames, exchanges);
  UniquePublicExchanges selectedPublicExchanges;
  selectedPublicExchanges.reserve(selectedExchanges.size());
  std::transform(selectedExchanges.begin(), selectedExchanges.end(),
                 std::inserter(selectedPublicExchanges, selectedPublicExchanges.end()),
                 [](Exchange *e) { return std::addressof(e->apiPublic()); });
  return selectedPublicExchanges;
}

template <class ExchangeNameT>
std::string ConstructAccumulatedExchangeNames(std::span<const ExchangeNameT> exchangeNames) {
  std::string exchangesStr(exchangeNames.empty() ? "all" : "");
  for (const ExchangeNameT &exchangeName : exchangeNames) {
    if (!exchangesStr.empty()) {
      exchangesStr.push_back(',');
    }
    exchangesStr.append(ToString(exchangeName));
  }
  return exchangesStr;
}
}  // namespace

Coincenter::Coincenter(settings::RunMode runMode)
    : _coincenterInfo(runMode),
      _cryptowatchAPI(runMode),
      _fiatConverter(std::chrono::hours(8)),
      _apiKeyProvider(runMode),
      _binancePublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _bithumbPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _huobiPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _krakenPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _upbitPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI) {
  for (std::string_view exchangeName : kSupportedExchanges) {
    api::ExchangePublic *exchangePublic;
    if (exchangeName == "binance") {
      exchangePublic = std::addressof(_binancePublic);
    } else if (exchangeName == "bithumb") {
      exchangePublic = std::addressof(_bithumbPublic);
    } else if (exchangeName == "huobi") {
      exchangePublic = std::addressof(_huobiPublic);
    } else if (exchangeName == "kraken") {
      exchangePublic = std::addressof(_krakenPublic);
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
        if (exchangeName == "binance") {
          exchangePrivate = std::addressof(_binancePrivates.emplace_front(_coincenterInfo, _binancePublic, apiKey));
        } else if (exchangeName == "bithumb") {
          exchangePrivate = std::addressof(_bithumbPrivates.emplace_front(_coincenterInfo, _bithumbPublic, apiKey));
        } else if (exchangeName == "huobi") {
          exchangePrivate = std::addressof(_huobiPrivates.emplace_front(_coincenterInfo, _huobiPublic, apiKey));
        } else if (exchangeName == "kraken") {
          exchangePrivate = std::addressof(_krakenPrivates.emplace_front(_coincenterInfo, _krakenPublic, apiKey));
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
  if (opts.marketForOrderBook != Market()) {
    std::optional<int> depth;
    if (opts.orderbookDepth != 0) {
      depth = opts.orderbookDepth;
    }
    Coincenter::MarketOrderBookConversionRates marketOrderBooksConversionRates =
        getMarketOrderBooks(opts.marketForOrderBook, opts.orderBookExchanges, opts.orderbookCur, depth);
    int orderBookPos = 0;
    for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBooksConversionRates) {
      log::info("Order book of {} on {} requested{}{}", opts.marketForOrderBook.str(), exchangeName,
                optConversionRate ? " with conversion rate " : "", optConversionRate ? optConversionRate->str() : "");

      if (optConversionRate) {
        marketOrderBook.print(std::cout, exchangeName, *optConversionRate);
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

  if (opts.marketForConversionPath != Market()) {
    printConversionPath(opts.conversionPathExchanges, opts.marketForConversionPath);
  }

  if (opts.balanceForAll || !opts.balancePrivateExchanges.empty()) {
    printBalance(opts.balancePrivateExchanges, opts.balanceCurrencyCode);
  }

  if (!opts.startTradeAmount.isZero()) {
    log::info("Trade {} into {} on {} requested", opts.startTradeAmount.str(), opts.toTradeCurrency.str(),
              opts.tradePrivateExchangeName.str());
    log::info(opts.tradeOptions.str());
    MonetaryAmount startAmount = opts.startTradeAmount;
    MonetaryAmount toAmount =
        trade(startAmount, opts.toTradeCurrency, opts.tradePrivateExchangeName, opts.tradeOptions);
    log::info("**** Traded {} into {} ****", (opts.startTradeAmount - startAmount).str(), toAmount.str());
  }

  if (!opts.amountToWithdraw.isZero()) {
    log::info("Withdraw gross {} from {} to {} requested", opts.amountToWithdraw.str(),
              opts.withdrawFromExchangeName.str(), opts.withdrawToExchangeName.str());
    withdraw(opts.amountToWithdraw, opts.withdrawFromExchangeName, opts.withdrawToExchangeName);
  }

  if (opts.withdrawFeeCur != CurrencyCode()) {
    printWithdrawFees(opts.withdrawFeeCur, opts.withdrawFeeExchanges);
  }

  updateFileCaches();
}

Coincenter::MarketOrderBookConversionRates Coincenter::getMarketOrderBooks(
    Market m, std::span<const PublicExchangeName> exchangeNames, CurrencyCode equiCurrencyCode,
    std::optional<int> depth) {
  MarketOrderBookConversionRates ret;
  for (api::ExchangePublic *e : RetrieveUniquePublicExchanges(exchangeNames, _exchanges)) {
    // Do not check if market exists when exchange names are specified to save API call
    if (!exchangeNames.empty() || e->queryTradableMarkets().contains(m)) {
      std::optional<MonetaryAmount> optConversionRate =
          equiCurrencyCode == CurrencyCode::kNeutral
              ? std::nullopt
              : e->convertAtAveragePrice(MonetaryAmount("1", m.quote()), equiCurrencyCode);
      ret.emplace_back(e->name(), MarketOrderBook(depth ? e->queryOrderBook(m, *depth) : e->queryOrderBook(m)),
                       optConversionRate);
    }
  }
  return ret;
}

BalancePortfolio Coincenter::getBalance(std::span<const PrivateExchangeName> privateExchangeNames,
                                        CurrencyCode equiCurrency) {
  bool balanceForAll = privateExchangeNames.empty();
  std::optional<CurrencyCode> optEquiCur = _coincenterInfo.fiatCurrencyIfStableCoin(equiCurrency);
  if (optEquiCur) {
    log::warn("Consider {} instead of stable coin {} as equivalent currency", optEquiCur->str(), equiCurrency.str());
    equiCurrency = *optEquiCur;
  }

  cct::vector<Exchange *> balanceExchanges;
  for (Exchange &exchange : _exchanges) {
    const bool computeBalance = (balanceForAll && exchange.hasPrivateAPI()) ||
                                std::any_of(privateExchangeNames.begin(), privateExchangeNames.end(),
                                            [&exchange](const PrivateExchangeName &privateExchangeName) {
                                              return exchange.matchesKeyNameWildcard(privateExchangeName);
                                            });
    if (computeBalance) {
      balanceExchanges.push_back(std::addressof(exchange));
    }
  }

  cct::vector<BalancePortfolio> subRet(balanceExchanges.size());
  std::transform(std::execution::par, balanceExchanges.begin(), balanceExchanges.end(), subRet.begin(),
                 [equiCurrency](Exchange *e) { return e->apiPrivate().queryAccountBalance(equiCurrency); });
  BalancePortfolio ret;
  for (const BalancePortfolio &sub : subRet) {
    ret.merge(sub);
  }
  return ret;
}

void Coincenter::printBalance(const PrivateExchangeNames &privateExchangeNames, CurrencyCode balanceCurrencyCode) {
  log::info("Query balance from {}",
            ConstructAccumulatedExchangeNames(std::span<const PrivateExchangeName>(privateExchangeNames)));
  BalancePortfolio portfolio = getBalance(privateExchangeNames, balanceCurrencyCode);
  portfolio.print(std::cout);
}

void Coincenter::printConversionPath(std::span<const PublicExchangeName> exchangeNames, Market m) {
  log::info("Query {} conversion path from {}", m.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  VariadicTable<std::string, std::string> vt({"Exchange", "Fastest conversion path"});
  for (api::ExchangePublic *e : RetrieveUniquePublicExchanges(exchangeNames, _exchanges)) {
    std::string conversionPathStr;
    api::ExchangePublic::Currencies conversionPath = e->findFastestConversionPath(m);
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

void Coincenter::printWithdrawFees(CurrencyCode currencyCode, std::span<const PublicExchangeName> exchangeNames) {
  log::info("{} withdraw fees for {}", currencyCode.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicExchanges selectedPublicExchanges = RetrieveUniquePublicExchanges(exchangeNames, _exchanges);
  cct::vector<api::ExchangePublic::WithdrawalFeeMap> withdrawFeesPerExchange(selectedPublicExchanges.size());
  std::transform(std::execution::par, selectedPublicExchanges.begin(), selectedPublicExchanges.end(),
                 std::begin(withdrawFeesPerExchange),
                 [currencyCode](api::ExchangePublic *e) { return e->queryWithdrawalFees(); });
  VariadicTable<std::string, std::string> vt({"Exchange", "Withdraw fee"});
  int exchangePos = 0;
  for (const api::ExchangePublic::WithdrawalFeeMap &withdrawFeesMap : withdrawFeesPerExchange) {
    auto foundIt = withdrawFeesMap.find(currencyCode);
    if (foundIt != withdrawFeesMap.end()) {
      vt.addRow(std::string(selectedPublicExchanges.begin()[exchangePos++]->name()), foundIt->second.str());
    }
  }
  vt.print();
}

PublicExchangeNames Coincenter::getPublicExchangeNames() const {
  PublicExchangeNames ret;
  ret.reserve(_exchanges.size());
  std::transform(std::begin(_exchanges), std::end(_exchanges), std::back_inserter(ret),
                 [](const Exchange &e) { return std::string(e.name()); });
  return ret;
}

void Coincenter::updateFileCaches() const {
  log::debug("Store all cache files");
  _cryptowatchAPI.updateCacheFile();
  _fiatConverter.updateCacheFile();
  std::for_each(_exchanges.begin(), _exchanges.end(), [](const Exchange &e) { e.updateCacheFile(); });
}

}  // namespace cct
