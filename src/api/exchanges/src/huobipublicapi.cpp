#include "huobipublicapi.hpp"

#include <algorithm>
#include <cassert>
#include <execution>
#include <thread>
#include <unordered_map>

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "monetaryamount.hpp"
#include "toupperlower.hpp"

namespace cct::api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, const CurlPostData& curlPostData = CurlPostData()) {
  string url(HuobiPublic::kUrlBase);
  url.append(endpoint);
  if (!curlPostData.empty()) {
    url.push_back('?');
    url.append(curlPostData.str());
  }
  json dataJson = json::parse(curlHandle.query(url, CurlOptions(HttpRequestType::kGet, HuobiPublic::kUserAgent)));
  bool returnData = dataJson.contains("data");
  if (!returnData && !dataJson.contains("tick")) {
    throw exception("No data for Huobi public endpoint");
  }
  return returnData ? dataJson["data"] : dataJson["tick"];
}

}  // namespace

HuobiPublic::HuobiPublic(const CoincenterInfo& config, FiatConverter& fiatConverter,
                         api::CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("huobi", fiatConverter, cryptowatchAPI, config),
      _exchangeInfo(config.exchangeInfo(_name)),
      _curlHandle(config.metricGatewayPtr(), _exchangeInfo.minPublicQueryDelay(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault),
          _curlHandle),
      _marketsCache(CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kMarkets), _cachedResultVault),
                    _curlHandle, _exchangeInfo),
      _allOrderBooksCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAllOrderBooks), _cachedResultVault),
          _marketsCache, _curlHandle, _exchangeInfo),
      _orderbookCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kOrderBook), _cachedResultVault),
          _curlHandle, _exchangeInfo),
      _tradedVolumeCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kTradedVolume), _cachedResultVault),
          _curlHandle),
      _tickerCache(CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kLastPrice), _cachedResultVault),
                   _curlHandle) {}

json HuobiPublic::TradableCurrenciesFunc::operator()() { return PublicQuery(_curlHandle, "/v2/reference/currencies"); }

CurrencyExchangeFlatSet HuobiPublic::queryTradableCurrencies() {
  const json& result = _tradableCurrenciesCache.get();
  CurrencyExchangeVector currencies;
  for (const json& curDetail : result) {
    std::string_view statusStr = curDetail["instStatus"].get<std::string_view>();
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    if (statusStr != "normal") {
      log::debug("{} is {} from Huobi", curStr, statusStr);
      continue;
    }
    bool foundChainWithSameName = false;
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    for (const json& chainDetail : curDetail["chains"]) {
      std::string_view chainName = chainDetail["chain"].get<std::string_view>();
      std::string_view displayName = chainDetail["displayName"].get<std::string_view>();
      if (CurrencyCode(chainName) != cur && CurrencyCode(displayName) != cur) {
        log::debug("Discarding chain {}", chainName);
        continue;
      }
      std::string_view depositAllowedStr = chainDetail["depositStatus"].get<std::string_view>();
      std::string_view withdrawAllowedStr = chainDetail["withdrawStatus"].get<std::string_view>();
      CurrencyExchange newCurrency(cur, curStr, curStr,
                                   depositAllowedStr == "allowed" ? CurrencyExchange::Deposit::kAvailable
                                                                  : CurrencyExchange::Deposit::kUnavailable,
                                   withdrawAllowedStr == "allowed" ? CurrencyExchange::Withdraw::kAvailable
                                                                   : CurrencyExchange::Withdraw::kUnavailable,
                                   _cryptowatchApi.queryIsCurrencyCodeFiat(cur) ? CurrencyExchange::Type::kFiat
                                                                                : CurrencyExchange::Type::kCrypto);

      log::debug("Retrieved Huobi Currency {}", newCurrency.str());
      currencies.push_back(std::move(newCurrency));

      foundChainWithSameName = true;
      break;
    }
    if (!foundChainWithSameName) {
      log::debug("Cannot find {} main chain in Huobi, discarding currency", cur.str());
    }
  }
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} Huobi currencies", ret.size());
  return ret;
}

std::pair<ExchangePublic::MarketSet, HuobiPublic::MarketsFunc::MarketInfoMap> HuobiPublic::MarketsFunc::operator()() {
  json result = PublicQuery(_curlHandle, "/v1/common/symbols");

  MarketSet markets;
  MarketInfoMap marketInfoMap;

  markets.reserve(static_cast<MarketSet::size_type>(result.size()));

  const ExchangeInfo::CurrencySet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();

  for (const json& marketDetails : result) {
    std::string_view baseAsset = marketDetails["base-currency"].get<std::string_view>();
    std::string_view quoteAsset = marketDetails["quote-currency"].get<std::string_view>();
    if (excludedCurrencies.contains(baseAsset) || excludedCurrencies.contains(quoteAsset)) {
      log::trace("Discard {}-{} excluded by config", baseAsset, quoteAsset);
      continue;
    }
    std::string_view apiTradingStr = marketDetails["api-trading"].get<std::string_view>();
    if (apiTradingStr != "enabled") {
      log::trace("Trading is {} for market {}-{}", apiTradingStr, baseAsset, quoteAsset);
      continue;
    }
    std::string_view stateStr = marketDetails["state"].get<std::string_view>();
    if (stateStr != "online") {  // Possible values are [online，pre-online,offline,suspend]
      log::trace("Trading is {} for market {}-{}", stateStr, baseAsset, quoteAsset);
      continue;
    }
    if (baseAsset.size() > CurrencyCode::kAcronymMaxLen || quoteAsset.size() > CurrencyCode::kAcronymMaxLen) {
      log::trace("Discard {}-{} as one asset is too long", baseAsset, quoteAsset);
      continue;
    }
    log::trace("Accept {}-{} Huobi asset pair", baseAsset, quoteAsset);
    CurrencyCode base(baseAsset);
    CurrencyCode quote(quoteAsset);
    Market m(base, quote);
    markets.insert(m);

    int8_t volNbDec = marketDetails["amount-precision"].get<int8_t>();
    int8_t priNbDec = marketDetails["price-precision"].get<int8_t>();
    // value = vol * pri in quote currency
    MarketInfo marketInfo;

    marketInfo.volAndPriNbDecimals = VolAndPriNbDecimals(volNbDec, priNbDec);

    marketInfo.minOrderValue = MonetaryAmount(marketDetails["min-order-value"].get<double>(), m.quote());
    if (marketDetails.contains("max-order-value")) {  // in USDT
      marketInfo.maxOrderValueUSDT = MonetaryAmount(marketDetails["max-order-value"].get<double>(), "USDT");
    }

    marketInfo.limitMinOrderAmount = MonetaryAmount(marketDetails["limit-order-min-order-amt"].get<double>(), m.base());
    marketInfo.limitMaxOrderAmount = MonetaryAmount(marketDetails["limit-order-max-order-amt"].get<double>(), m.base());

    marketInfo.sellMarketMinOrderAmount =
        MonetaryAmount(marketDetails["sell-market-min-order-amt"].get<double>(), m.base());
    marketInfo.sellMarketMaxOrderAmount =
        MonetaryAmount(marketDetails["sell-market-max-order-amt"].get<double>(), m.base());

    marketInfo.buyMarketMaxOrderValue =
        MonetaryAmount(marketDetails["buy-market-max-order-value"].get<double>(), m.quote());

    marketInfoMap.insert_or_assign(m, std::move(marketInfo));
  }
  log::info("Retrieved huobi {} markets", markets.size());
  return {std::move(markets), std::move(marketInfoMap)};
}

ExchangePublic::WithdrawalFeeMap HuobiPublic::queryWithdrawalFees() {
  WithdrawalFeeMap ret;
  for (const json& curDetail : _tradableCurrenciesCache.get()) {
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    bool foundChainWithSameName = false;
    for (const json& chainDetail : curDetail["chains"]) {
      std::string_view chainName = chainDetail["chain"].get<std::string_view>();
      std::string_view displayName = chainDetail["displayName"].get<std::string_view>();
      if (CurrencyCode(chainName) != cur && CurrencyCode(displayName) != cur) {
        log::debug("Discarding chain '{}'", chainName);
        continue;
      }
      std::string_view withdrawFeeStr = chainDetail["transactFeeWithdraw"].get<std::string_view>();
      MonetaryAmount withdrawFee(withdrawFeeStr, cur);

      log::trace("Retrieved {} withdrawal fee {}", _name, withdrawFee.str());
      ret.insert_or_assign(cur, withdrawFee);

      foundChainWithSameName = true;
      break;
    }
    if (!foundChainWithSameName) {
      log::debug("Cannot find '{}' main chain in {}, discarding currency", curStr, _name);
    }
  }

  log::info("Retrieved {} withdrawal fees for {} coins", _name, ret.size());
  assert(!ret.empty());
  return ret;
}

MonetaryAmount HuobiPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  for (const json& curDetail : _tradableCurrenciesCache.get()) {
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    if (cur != currencyCode) {
      continue;
    }
    for (const json& chainDetail : curDetail["chains"]) {
      std::string_view chainName = chainDetail["chain"].get<std::string_view>();
      if (chainName == cur) {
        return MonetaryAmount(chainDetail["transactFeeWithdraw"].get<std::string_view>(), cur);
      }
    }
  }
  throw exception("Unable to find withdrawal fee for " + string(currencyCode.str()));
}

ExchangePublic::MarketOrderBookMap HuobiPublic::AllOrderBooksFunc::operator()(int depth) {
  MarketOrderBookMap ret;
  const auto& [markets, marketInfoMap] = _marketsCache.get();
  using HuobiAssetPairToStdMarketMap = std::unordered_map<string, Market>;
  HuobiAssetPairToStdMarketMap huobiAssetPairToStdMarketMap;
  huobiAssetPairToStdMarketMap.reserve(markets.size());
  for (Market m : markets) {
    huobiAssetPairToStdMarketMap.insert_or_assign(m.assetsPairStrUpper(), m);
  }
  for (const json& tickerDetails : PublicQuery(_curlHandle, "/market/tickers")) {
    string upperMarket = ToUpper(tickerDetails["symbol"].get<std::string_view>());
    auto it = huobiAssetPairToStdMarketMap.find(upperMarket);
    if (it == huobiAssetPairToStdMarketMap.end()) {
      continue;
    }
    Market m = it->second;
    MonetaryAmount askPri(tickerDetails["ask"].get<double>(), m.quote());
    MonetaryAmount bidPri(tickerDetails["bid"].get<double>(), m.quote());
    MonetaryAmount askVol(tickerDetails["askSize"].get<double>(), m.base());
    MonetaryAmount bidVol(tickerDetails["bidSize"].get<double>(), m.base());

    if (bidVol.isZero() || askVol.isZero()) {
      log::trace("No volume for {}", m.str());
      continue;
    }

    const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(m)->second;

    ret.insert_or_assign(m, MarketOrderBook(askPri, askVol, bidPri, bidVol, marketInfo.volAndPriNbDecimals, depth));
  }

  log::info("Retrieved Huobi ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook HuobiPublic::OrderBookFunc::operator()(Market m, int depth) {
  // Huobi has a fixed range of authorized values for depth
  CurlPostData postData{{"symbol", m.assetsPairStrLower()}, {"type", "step0"}};
  if (depth != kHuobiStandardOrderBookDefaultDepth) {
    static constexpr int kAuthorizedDepths[] = {5, 10, 20, kHuobiStandardOrderBookDefaultDepth};
    auto lb = std::ranges::lower_bound(kAuthorizedDepths, depth);
    if (lb == std::end(kAuthorizedDepths)) {
      log::warn("Invalid depth {}, default to {}", depth, kHuobiStandardOrderBookDefaultDepth);
    } else if (*lb != kHuobiStandardOrderBookDefaultDepth) {
      postData.append("depth", *lb);
    }
  }
  json asksAndBids = PublicQuery(_curlHandle, "/market/depth", postData);
  const json& asks = asksAndBids["asks"];
  const json& bids = asksAndBids["bids"];
  using OrderBookVec = vector<OrderBookLine>;
  OrderBookVec orderBookLines;
  orderBookLines.reserve(static_cast<OrderBookVec::size_type>(depth) * 2);
  for (auto asksOrBids : {std::addressof(bids), std::addressof(asks)}) {
    const bool isAsk = asksOrBids == std::addressof(asks);
    int n = 0;
    for (const auto& priceQuantityPair : *asksOrBids) {
      MonetaryAmount amount(priceQuantityPair.back().get<double>(), m.base());
      MonetaryAmount price(priceQuantityPair.front().get<double>(), m.quote());

      orderBookLines.emplace_back(amount, price, isAsk);
      if (++n == depth) {
        if (depth < static_cast<int>(asksOrBids->size())) {
          log::debug("Truncate number of {} prices in order book to {}", isAsk ? "ask" : "bid", depth);
        }
        break;
      }
    }
  }
  return MarketOrderBook(m, orderBookLines);
}

MonetaryAmount HuobiPublic::sanitizePrice(Market m, MonetaryAmount pri) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  MonetaryAmount sanitizedPri = pri;
  sanitizedPri.truncate(marketInfoMap.find(m)->second.volAndPriNbDecimals.priNbDecimals);
  if (sanitizedPri != pri) {
    log::warn("Sanitize price {} -> {}", pri.str(), sanitizedPri.str());
  }
  return sanitizedPri;
}

MonetaryAmount HuobiPublic::sanitizeVolume(Market m, CurrencyCode fromCurrencyCode, MonetaryAmount vol,
                                           MonetaryAmount sanitizedPrice, bool isTakerOrder) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(m)->second;
  MonetaryAmount sanitizedVol = vol;

  if (sanitizedVol.toNeutral() * sanitizedPrice < marketInfo.minOrderValue) {
    sanitizedVol = MonetaryAmount(marketInfo.minOrderValue / sanitizedPrice, sanitizedVol.currencyCode());
    MonetaryAmount step(1, sanitizedVol.currencyCode(), marketInfo.volAndPriNbDecimals.volNbDecimals);
    sanitizedVol = sanitizedVol.round(step, MonetaryAmount::RoundType::kUp);
  } else {
    sanitizedVol.truncate(marketInfo.volAndPriNbDecimals.volNbDecimals);
  }
  if (isTakerOrder) {
    if (fromCurrencyCode == m.base() && sanitizedVol < marketInfo.sellMarketMinOrderAmount) {
      sanitizedVol = marketInfo.sellMarketMinOrderAmount;
    }
  } else {
    if (sanitizedVol < marketInfo.limitMinOrderAmount) {
      sanitizedVol = marketInfo.limitMinOrderAmount;
    }
  }
  if (sanitizedVol != vol) {
    log::warn("Sanitize volume {} -> {}", vol.str(), sanitizedVol.str());
  }
  return sanitizedVol;
}

MonetaryAmount HuobiPublic::TradedVolumeFunc::operator()(Market m) {
  json result = PublicQuery(_curlHandle, "/market/detail/merged", {{"symbol", m.assetsPairStrLower()}});
  double last24hVol = result["amount"].get<double>();
  return MonetaryAmount(last24hVol, m.base());
}

HuobiPublic::LastTradesVector HuobiPublic::queryLastTrades(Market m, int nbTrades) {
  nbTrades = std::min(nbTrades, 2000);  // max authorized
  nbTrades = std::max(nbTrades, 1);     // min authorized
  json result =
      PublicQuery(_curlHandle, "/market/history/trade", {{"symbol", m.assetsPairStrLower()}, {"size", nbTrades}});
  LastTradesVector ret;
  for (const json& detail : result) {
    auto dataDetails = detail.find("data");
    if (dataDetails != detail.end()) {
      for (const json& detail2 : *dataDetails) {
        MonetaryAmount amount(detail2["amount"].get<double>(), m.base());
        MonetaryAmount price(detail2["price"].get<double>(), m.quote());
        int64_t millisecondsSinceEpoch = detail2["ts"].get<int64_t>();
        TradeSide tradeSide =
            detail2["direction"].get<std::string_view>() == "buy" ? TradeSide::kBuy : TradeSide::kSell;

        ret.emplace_back(tradeSide, amount, price, TimePoint(std::chrono::milliseconds(millisecondsSinceEpoch)));
      }
    }
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount HuobiPublic::TickerFunc::operator()(Market m) {
  json result = PublicQuery(_curlHandle, "/market/trade", {{"symbol", m.assetsPairStrLower()}});
  double lastPrice = result["data"].front()["price"].get<double>();
  return MonetaryAmount(lastPrice, m.quote());
}
}  // namespace cct::api
