#include "krakenpublicapi.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "cachedresult.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "httprequesttype.hpp"
#include "kraken-schema.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "order-book-line.hpp"
#include "permanentcurloptions.hpp"
#include "public-trade-vector.hpp"
#include "request-retry.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct::api {
namespace {

template <class T>
T PublicQuery(CurlHandle& curlHandle, std::string_view method, CurlPostData&& postData = CurlPostData()) {
  RequestRetry requestRetry(curlHandle, CurlOptions(HttpRequestType::kGet, std::move(postData)));
  return requestRetry.query<T>(method, [](const T& response) {
    if constexpr (amc::is_detected<schema::kraken::has_error_t, T>::value) {
      if (!response.error.empty()) {
        log::error("Kraken error(s): {}", response.error.front());
        return RequestRetry::Status::kResponseError;
      }
    }

    return RequestRetry::Status::kResponseOK;
  });
}

bool CheckCurrencyExchange(std::string_view krakenEntryCurrencyCode, std::string_view krakenAltName,
                           const CurrencyCodeSet& excludedCurrencies, const CoincenterInfo& config) {
  if (krakenAltName.ends_with(".HOLD")) {
    // These are special tokens for holding
    log::trace("Discard {} which are special tokens for holding process", krakenAltName);
    return false;
  }
  if (krakenAltName.ends_with(".M")) {
    log::trace("Discard {} which are special tokens for margin", krakenAltName);
    return false;
  }
  if (krakenAltName.ends_with(".S")) {
    log::trace("Discard {} which are special tokens for staking", krakenAltName);
    return false;
  }

  // Kraken manages 2 versions of Augur, do not take first version into account to avoid issues of acronym names
  // between exchanges
  static constexpr bool kAvoidAugurV1AndKeepAugurV2 = true;

  if constexpr (kAvoidAugurV1AndKeepAugurV2) {
    if (krakenEntryCurrencyCode == "XREP") {
      log::trace("Discard {} favored by Augur V2", krakenEntryCurrencyCode);
      return false;
    }
  }

  CurrencyCode standardCode(config.standardizeCurrencyCode(krakenAltName));
  if (excludedCurrencies.contains(standardCode)) {
    // Forbidden currency, do not consider its market
    log::trace("Discard {} excluded by config", standardCode);
    return false;
  }
  return true;
}

}  // namespace

KrakenPublic::KrakenPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CommonAPI& commonAPI)
    : ExchangePublic(ExchangeNameEnum::kraken, fiatConverter, commonAPI, config),
      _curlHandle(kUrlBase, config.metricGatewayPtr(), permanentCurlOptionsBuilder().build(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::currencies).duration,
                              _cachedResultVault),
          config, commonAPI, _curlHandle, exchangeConfig().asset),
      _marketsCache(CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::markets).duration,
                                        _cachedResultVault),
                    _tradableCurrenciesCache, config, _curlHandle, exchangeConfig().asset),
      _allOrderBooksCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::allOrderBooks).duration,
                              _cachedResultVault),
          _tradableCurrenciesCache, _marketsCache, config, _curlHandle),
      _orderBookCache(CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::orderBook).duration,
                                          _cachedResultVault),
                      _tradableCurrenciesCache, _marketsCache, _curlHandle),
      _tickerCache(
          CachedResultOptions(std::min(exchangeConfig().query.updateFrequency.at(QueryType::tradedVolume).duration,
                                       exchangeConfig().query.updateFrequency.at(QueryType::lastPrice).duration),
                              _cachedResultVault),
          _tradableCurrenciesCache, _curlHandle) {}

bool KrakenPublic::healthCheck() {
  const auto result = PublicQuery<schema::kraken::SystemStatus>(_curlHandle, "/public/SystemStatus");
  log::info("{} status: {}", name(), result.result.status);
  return result.result.status == "online";
}

std::optional<MonetaryAmount> KrakenPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  return _commonApi.tryQueryWithdrawalFee(exchangeNameEnum(), currencyCode);
}

CurrencyExchangeFlatSet KrakenPublic::TradableCurrenciesFunc::operator()() {
  const auto result = PublicQuery<schema::kraken::Assets>(_curlHandle, "/public/Assets");
  const CurrencyCodeSet& excludedCurrencies = _assetConfig.allExclude;

  CurrencyExchangeVector currencies;
  for (const auto& [krakenAssetName, value] : result.result) {
    std::string_view altCodeStr = value.altname;
    if (!CheckCurrencyExchange(krakenAssetName, altCodeStr, excludedCurrencies, _coincenterInfo)) {
      continue;
    }
    CurrencyCode standardCode(_coincenterInfo.standardizeCurrencyCode(altCodeStr));
    CurrencyExchange newCurrency(standardCode, CurrencyCode(krakenAssetName), CurrencyCode(altCodeStr),
                                 CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                                 _commonApi.queryIsCurrencyCodeFiat(standardCode) ? CurrencyExchange::Type::kFiat
                                                                                  : CurrencyExchange::Type::kCrypto);

    log::trace("Retrieved kraken Currency {}", newCurrency.str());
    currencies.push_back(std::move(newCurrency));
  }
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::debug("Retrieved {} kraken currencies", ret.size());
  return ret;
}

std::pair<MarketSet, KrakenPublic::MarketsFunc::MarketInfoMap> KrakenPublic::MarketsFunc::operator()() {
  const auto result = PublicQuery<schema::kraken::AssetPairs>(_curlHandle, "/public/AssetPairs");
  std::pair<MarketSet, MarketInfoMap> ret;
  ret.first.reserve(static_cast<MarketSet::size_type>(result.result.size()));
  ret.second.reserve(result.result.size());
  const CurrencyCodeSet& excludedCurrencies = _assetConfig.allExclude;
  const CurrencyExchangeFlatSet& currencies = _tradableCurrenciesCache.get();
  for (const auto& [key, value] : result.result) {
    if (value.ordermin.isDefault()) {
      log::debug("Discard market {} as it does not contain min order information", key);
      continue;
    }
    std::string_view krakenBaseStr = value.base;
    CurrencyCode base(_coincenterInfo.standardizeCurrencyCode(krakenBaseStr));
    auto baseIt = currencies.find(base);
    if (baseIt == currencies.end()) {
      continue;
    }
    const CurrencyExchange& baseExchange = *baseIt;
    if (!CheckCurrencyExchange(krakenBaseStr, baseExchange.altStr(), excludedCurrencies, _coincenterInfo)) {
      continue;
    }

    std::string_view krakenQuoteStr = value.quote;
    CurrencyCode quote(_coincenterInfo.standardizeCurrencyCode(krakenQuoteStr));
    auto quoteIt = currencies.find(quote);
    if (quoteIt == currencies.end()) {
      continue;
    }
    const CurrencyExchange& quoteExchange = *quoteIt;
    if (!CheckCurrencyExchange(krakenQuoteStr, quoteExchange.altStr(), excludedCurrencies, _coincenterInfo)) {
      continue;
    }
    auto mkIt = ret.first.emplace(base, quote).first;
    log::trace("Retrieved Kraken market {}", *mkIt);
    MonetaryAmount orderMin(value.ordermin, base);
    ret.second.insert_or_assign(*mkIt, MarketInfo{{value.lot_decimals, value.pair_decimals}, orderMin});
  }
  log::debug("Retrieved {} markets from kraken", ret.first.size());
  return ret;
}

MarketOrderBookMap KrakenPublic::AllOrderBooksFunc::operator()(int depth) {
  const CurrencyExchangeFlatSet& krakenCurrencies = _tradableCurrenciesCache.get();
  const auto& [markets, marketInfoMap] = _marketsCache.get();

  using KrakenAssetPairToStdMarketMap = std::unordered_map<string, Market>;
  KrakenAssetPairToStdMarketMap krakenAssetPairToStdMarketMap;
  krakenAssetPairToStdMarketMap.reserve(markets.size());

  string allAssetPairs;
  MarketOrderBookMap ret;
  ret.reserve(markets.size());
  for (Market mk : markets) {
    auto it = krakenCurrencies.find(mk.base());
    if (it == krakenCurrencies.end()) {
      throw exception("Cannot find {} in Kraken currencies", mk.base());
    }
    CurrencyExchange krakenCurrencyExchangeBase = *it;
    it = krakenCurrencies.find(mk.quote());
    if (it == krakenCurrencies.end()) {
      throw exception("Cannot find {} in Kraken currencies", mk.quote());
    }
    CurrencyExchange krakenCurrencyExchangeQuote = *it;
    Market krakenMarket(krakenCurrencyExchangeBase.altCode(), krakenCurrencyExchangeQuote.altCode());
    string assetPairStr = krakenMarket.assetsPairStrUpper();
    if (!allAssetPairs.empty()) {
      allAssetPairs.push_back(',');
    }
    allAssetPairs.append(assetPairStr);
    krakenAssetPairToStdMarketMap.insert_or_assign(assetPairStr, mk);
    krakenAssetPairToStdMarketMap.insert_or_assign(
        Market(krakenCurrencyExchangeBase.exchangeCode(), krakenCurrencyExchangeQuote.exchangeCode())
            .assetsPairStrUpper(),
        mk);
  }
  const auto result = PublicQuery<schema::kraken::Ticker>(_curlHandle, "/public/Ticker", {{"pair", allAssetPairs}});
  const auto time = Clock::now();
  for (const auto& [krakenAssetPair, assetPairDetails] : result.result) {
    if (krakenAssetPairToStdMarketMap.find(krakenAssetPair) == krakenAssetPairToStdMarketMap.end()) {
      log::error("Unable to find {}", krakenAssetPair);
      continue;
    }

    Market mk = krakenAssetPairToStdMarketMap.find(krakenAssetPair)->second;
    mk =
        Market(_coincenterInfo.standardizeCurrencyCode(mk.base()), _coincenterInfo.standardizeCurrencyCode(mk.quote()));
    //  a = ask array(<price>, <whole lot volume>, <lot volume>)
    //  b = bid array(<price>, <whole lot volume>, <lot volume>)
    const auto& askDetails = assetPairDetails.a;
    const auto& bidDetails = assetPairDetails.b;

    MonetaryAmount askPri(askDetails[0], mk.quote());
    MonetaryAmount bidPri(bidDetails[0], mk.quote());
    MonetaryAmount askVol(askDetails[2], mk.base());
    MonetaryAmount bidVol(bidDetails[2], mk.base());

    const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(mk)->second;

    if (bidVol != 0 && askVol != 0) {
      ret.insert_or_assign(
          mk, MarketOrderBook(time, askPri, askVol, bidPri, bidVol, marketInfo.volAndPriNbDecimals, depth));
    }
  }

  log::info("Retrieved ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook KrakenPublic::OrderBookFunc::operator()(Market mk, int count) {
  CurrencyExchangeFlatSet krakenCurrencies = _tradableCurrenciesCache.get();
  auto lb = krakenCurrencies.find(mk.base());
  if (lb == krakenCurrencies.end()) {
    throw exception("Cannot find {} in Kraken currencies", mk.base());
  }
  CurrencyExchange krakenCurrencyExchangeBase = *lb;
  lb = krakenCurrencies.find(mk.quote());
  if (lb == krakenCurrencies.end()) {
    throw exception("Cannot find {} in Kraken currencies", mk.quote());
  }
  CurrencyExchange krakenCurrencyExchangeQuote = *lb;
  string krakenAssetPair = krakenCurrencyExchangeBase.altStr();
  krakenAssetPair.append(krakenCurrencyExchangeQuote.altStr());

  MarketOrderBookLines orderBookLines;

  const auto result =
      PublicQuery<schema::kraken::Depth>(_curlHandle, "/public/Depth", {{"pair", krakenAssetPair}, {"count", count}});
  const auto dataIt = result.result.find(krakenAssetPair);
  const auto nowTime = Clock::now();
  if (dataIt != result.result.end()) {
    const auto& asks = dataIt->second.asks;
    const auto& bids = dataIt->second.bids;
    orderBookLines.reserve(asks.size() + bids.size());
    for (const auto asksOrBids : {&asks, &bids}) {
      const auto type = asksOrBids == &asks ? OrderBookLine::Type::kAsk : OrderBookLine::Type::kBid;
      for (const auto& priceQuantityTuple : *asksOrBids) {
        MonetaryAmount price(std::get<string>(priceQuantityTuple[0]), mk.quote());
        MonetaryAmount amount(std::get<string>(priceQuantityTuple[1]), mk.base());

        orderBookLines.push(amount, price, type);
      }
    }
  }

  const auto volAndPriNbDecimals = _marketsCache.get().second.find(mk)->second.volAndPriNbDecimals;
  return MarketOrderBook(nowTime, mk, orderBookLines, volAndPriNbDecimals);
}

namespace {
Market GetKrakenMarketOrDefault(const CurrencyExchangeFlatSet& currencies, Market mk) {
  const auto krakenBaseIt = currencies.find(mk.base());
  const auto krakenQuoteIt = currencies.find(mk.quote());
  if (krakenBaseIt != currencies.end() && krakenQuoteIt != currencies.end()) {
    return {krakenBaseIt->altCode(), krakenQuoteIt->altCode()};
  }
  return {};
}
}  // namespace

KrakenPublic::TickerFunc::Last24hTradedVolumeAndLatestPricePair KrakenPublic::TickerFunc::operator()(Market mk) {
  const Market krakenMarket = GetKrakenMarketOrDefault(_tradableCurrenciesCache.get(), mk);

  if (krakenMarket.isDefined()) {
    const auto krakenPair = krakenMarket.assetsPairStrUpper();
    const auto result = PublicQuery<schema::kraken::Ticker>(_curlHandle, "/public/Ticker", {{"pair", krakenPair}});
    const auto dataIt = result.result.find(krakenPair);
    if (dataIt != result.result.end()) {
      return {MonetaryAmount(dataIt->second.v[1], mk.base()), MonetaryAmount(dataIt->second.c[0], mk.quote())};
    }
  }

  return {MonetaryAmount(0, mk.base()), MonetaryAmount(0, mk.quote())};
}

PublicTradeVector KrakenPublic::queryLastTrades(Market mk, int nbLastTrades) {
  PublicTradeVector ret;

  const Market krakenMarket = GetKrakenMarketOrDefault(_tradableCurrenciesCache.get(), mk);
  if (krakenMarket.isDefined()) {
    const auto krakenPair = krakenMarket.assetsPairStrUpper();
    const auto result = PublicQuery<schema::kraken::Trades>(_curlHandle, "/public/Trades",
                                                            {{"pair", krakenPair}, {"count", nbLastTrades}});

    const auto dataIt = result.result.find(krakenPair);

    if (dataIt != result.result.end()) {
      const auto& lastTrades = std::get<schema::kraken::Trades::Data>(dataIt->second);

      ret.reserve(static_cast<PublicTradeVector::size_type>(lastTrades.size()));
      for (const auto& det : lastTrades) {
        const MonetaryAmount price(std::get<string>(det[0]), mk.quote());
        const MonetaryAmount amount(std::get<string>(det[1]), mk.base());
        const auto millisecondsSinceEpoch = static_cast<int64_t>(std::get<double>(det[2]) * 1000);
        const TradeSide tradeSide = std::get<string>(det[3]) == "b" ? TradeSide::kBuy : TradeSide::kSell;

        ret.emplace_back(tradeSide, amount, price, TimePoint(milliseconds(millisecondsSinceEpoch)));
      }
      std::ranges::sort(ret);
    }
  }

  return ret;
}

}  // namespace cct::api
