#include "kucoinpublicapi.hpp"

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
#include "stringhelpers.hpp"
#include "toupperlower.hpp"

namespace cct {
namespace api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, const CurlPostData& curlPostData = CurlPostData()) {
  string url(KucoinPublic::kUrlBase);
  url.push_back('/');
  url.append(endpoint);
  if (!curlPostData.empty()) {
    url.push_back('?');
    url.append(curlPostData.str());
  }
  CurlOptions opts(HttpRequestType::kGet);
  opts.userAgent = KucoinPublic::kUserAgent;
  json dataJson = json::parse(curlHandle.query(url, opts));
  if (dataJson.contains("code") && dataJson["code"].get<std::string_view>() != "200000") {
    throw exception("Error in Kucoin REST API response");
  }
  return dataJson["data"];
}

}  // namespace

KucoinPublic::KucoinPublic(const CoincenterInfo& config, FiatConverter& fiatConverter,
                           api::CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("kucoin", fiatConverter, cryptowatchAPI, config),
      _exchangeInfo(config.exchangeInfo(_name)),
      _curlHandle(config.metricGatewayPtr(), _exchangeInfo.minPublicQueryDelay(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault),
          _curlHandle, _coincenterInfo, cryptowatchAPI),
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

KucoinPublic::TradableCurrenciesFunc::CurrencyInfoSet KucoinPublic::TradableCurrenciesFunc::operator()() {
  json result = PublicQuery(_curlHandle, "api/v1/currencies");
  vector<CurrencyInfo> currencyInfos;
  currencyInfos.reserve(static_cast<uint32_t>(result.size()));
  for (const json& curDetail : result) {
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    CurrencyExchange currencyExchange(
        cur, curStr, curStr,
        curDetail["isDepositEnabled"].get<bool>() ? CurrencyExchange::Deposit::kAvailable
                                                  : CurrencyExchange::Deposit::kUnavailable,
        curDetail["isWithdrawEnabled"].get<bool>() ? CurrencyExchange::Withdraw::kAvailable
                                                   : CurrencyExchange::Withdraw::kUnavailable,
        _cryptowatchApi.queryIsCurrencyCodeFiat(cur) ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto);

    log::debug("Retrieved Kucoin Currency {}", currencyExchange.str());

    currencyInfos.emplace_back(std::move(currencyExchange),
                               MonetaryAmount(curDetail["withdrawalMinSize"].get<std::string_view>(), cur),
                               MonetaryAmount(curDetail["withdrawalMinFee"].get<std::string_view>(), cur));
  }
  CurrencyInfoSet ret(std::move(currencyInfos));
  log::info("Retrieved {} Kucoin currencies", ret.size());
  return CurrencyInfoSet(std::move(ret));
}

CurrencyExchangeFlatSet KucoinPublic::queryTradableCurrencies() {
  const TradableCurrenciesFunc::CurrencyInfoSet& currencyInfoSet = _tradableCurrenciesCache.get();
  CurrencyExchangeFlatSet currencies;
  currencies.reserve(static_cast<CurrencyExchangeFlatSet::size_type>(currencyInfoSet.size()));

  std::transform(currencyInfoSet.begin(), currencyInfoSet.end(), std::inserter(currencies, currencies.end()),
                 [](const auto& currencyInfo) { return currencyInfo.currencyExchange; });

  return currencies;
}

std::pair<ExchangePublic::MarketSet, KucoinPublic::MarketsFunc::MarketInfoMap> KucoinPublic::MarketsFunc::operator()() {
  json result = PublicQuery(_curlHandle, "api/v1/symbols");

  MarketSet markets;
  MarketInfoMap marketInfoMap;

  markets.reserve(static_cast<MarketSet::size_type>(result.size()));

  const ExchangeInfo::CurrencySet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();

  for (const json& marketDetails : result) {
    const std::string_view baseAsset = marketDetails["baseCurrency"].get<std::string_view>();
    const std::string_view quoteAsset = marketDetails["quoteCurrency"].get<std::string_view>();
    const bool isEnabled = marketDetails["enableTrading"].get<bool>();
    if (!isEnabled) {
      log::trace("Trading is disabled for market {}-{}", baseAsset, quoteAsset);
      continue;
    }
    if (excludedCurrencies.contains(baseAsset) || excludedCurrencies.contains(quoteAsset)) {
      log::trace("Discard {}-{} excluded by config", baseAsset, quoteAsset);
      continue;
    }
    if (baseAsset.size() > CurrencyCode::kAcronymMaxLen || quoteAsset.size() > CurrencyCode::kAcronymMaxLen) {
      log::trace("Discard {}-{} as one asset is too long", baseAsset, quoteAsset);
      continue;
    }
    log::trace("Accept {}-{} Kucoin asset pair", baseAsset, quoteAsset);
    CurrencyCode base(baseAsset);
    CurrencyCode quote(quoteAsset);
    Market m(base, quote);
    markets.insert(m);

    MarketInfo& marketInfo = marketInfoMap[m];

    marketInfo.baseMinSize = MonetaryAmount(marketDetails["baseMinSize"].get<std::string_view>(), base);
    marketInfo.quoteMinSize = MonetaryAmount(marketDetails["quoteMinSize"].get<std::string_view>(), quote);
    marketInfo.baseMaxSize = MonetaryAmount(marketDetails["baseMaxSize"].get<std::string_view>(), base);
    marketInfo.quoteMaxSize = MonetaryAmount(marketDetails["quoteMaxSize"].get<std::string_view>(), quote);
    marketInfo.baseIncrement = MonetaryAmount(marketDetails["baseIncrement"].get<std::string_view>(), base);
    marketInfo.priceIncrement = MonetaryAmount(marketDetails["priceIncrement"].get<std::string_view>(), quote);
    marketInfo.feeCurrency = CurrencyCode(marketDetails["feeCurrency"].get<std::string_view>());
  }
  log::info("Retrieved Kucoin {} markets", markets.size());
  return {std::move(markets), std::move(marketInfoMap)};
}

ExchangePublic::WithdrawalFeeMap KucoinPublic::queryWithdrawalFees() {
  WithdrawalFeeMap ret;
  for (const TradableCurrenciesFunc::CurrencyInfo& curDetail : _tradableCurrenciesCache.get()) {
    ret.insert_or_assign(curDetail.currencyExchange.standardCode(), curDetail.withdrawalMinFee);
    log::trace("Retrieved {} withdrawal fee {}", _name, curDetail.withdrawalMinFee.str());
  }

  log::info("Retrieved {} withdrawal fees for {} coins", _name, ret.size());
  assert(!ret.empty());
  return ret;
}

MonetaryAmount KucoinPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  const auto& currencyInfoSet = _tradableCurrenciesCache.get();
  auto it = currencyInfoSet.find(TradableCurrenciesFunc::CurrencyInfo(currencyCode));
  if (it == currencyInfoSet.end()) {
    throw exception("Unable to find withdrawal fee for " + string(currencyCode.str()));
  }
  return MonetaryAmount(it->withdrawalMinFee, it->currencyExchange.standardCode());
}

ExchangePublic::MarketOrderBookMap KucoinPublic::AllOrderBooksFunc::operator()(int depth) {
  MarketOrderBookMap ret;
  const auto& [markets, marketInfoMap] = _marketsCache.get();
  json data = PublicQuery(_curlHandle, "api/v1/market/allTickers");
  for (const json& tickerDetails : data["ticker"]) {
    Market m(tickerDetails["symbol"].get<std::string_view>(), '-');
    if (!markets.contains(m)) {
      log::trace("Market {} is not present", m.assetsPairStr());
      continue;
    }
    MonetaryAmount askPri(tickerDetails["sell"].get<std::string_view>(), m.quote());
    MonetaryAmount bidPri(tickerDetails["buy"].get<std::string_view>(), m.quote());
    // There is no volume in the response, we need to emulate it, based on the 24h volume
    MonetaryAmount dayVolume(tickerDetails["vol"].get<std::string_view>(), m.base());
    if (dayVolume.isZero()) {
      log::trace("No volume for {}", m.assetsPairStr());
      continue;
    }
    const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(m)->second;

    // Use avg traded volume by second as ask/bid vol
    MonetaryAmount askVol =
        (dayVolume / (2 * 24 * 3600)).round(marketInfo.baseIncrement, MonetaryAmount::RoundType::kNearest);
    if (askVol.isZero()) {
      askVol = marketInfo.baseIncrement;
    }
    MonetaryAmount bidVol = askVol;

    VolAndPriNbDecimals volAndPriNbDecimals{marketInfo.baseIncrement.nbDecimals(),
                                            marketInfo.priceIncrement.nbDecimals()};

    ret.insert_or_assign(m, MarketOrderBook(askPri, askVol, bidPri, bidVol, volAndPriNbDecimals, depth));
  }

  log::info("Retrieved Kucoin ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook KucoinPublic::OrderBookFunc::operator()(Market m, int depth) {
  // Kucoin has a fixed range of authorized values for depth
  string symbol = m.assetsPairStr('-');
  CurlPostData postData{{"symbol", std::string_view(symbol)}};
  static constexpr int kAuthorizedDepths[] = {20, 100};
  auto lb = std::lower_bound(std::begin(kAuthorizedDepths), std::end(kAuthorizedDepths), depth);
  if (lb == std::end(kAuthorizedDepths)) {
    lb = std::next(std::end(kAuthorizedDepths), -1);
    log::error("Invalid depth {}, default to {}", depth, kKucoinStandardOrderBookDefaultDepth);
    depth = kKucoinStandardOrderBookDefaultDepth;
  } else {
    depth = *lb;
  }
  string endpoint("api/v1/market/orderbook/level2_");
  AppendString(endpoint, depth);

  json asksAndBids = PublicQuery(_curlHandle, endpoint, postData);
  const json& asks = asksAndBids["asks"];
  const json& bids = asksAndBids["bids"];
  using OrderBookVec = vector<OrderBookLine>;
  OrderBookVec orderBookLines;
  orderBookLines.reserve(static_cast<OrderBookVec::size_type>(asks.size() + bids.size()));
  for (auto asksOrBids : {std::addressof(asks), std::addressof(bids)}) {
    const bool isAsk = asksOrBids == std::addressof(asks);
    for (const auto& priceQuantityPair : *asksOrBids) {
      MonetaryAmount price(priceQuantityPair.front().get<std::string_view>(), m.quote());
      MonetaryAmount amount(priceQuantityPair.back().get<std::string_view>(), m.base());

      orderBookLines.emplace_back(amount, price, isAsk);
    }
  }
  return MarketOrderBook(m, orderBookLines);
}

MonetaryAmount KucoinPublic::sanitizePrice(Market m, MonetaryAmount pri) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(m)->second;
  MonetaryAmount sanitizedPri = pri;
  if (pri < marketInfo.priceIncrement) {
    sanitizedPri = marketInfo.priceIncrement;
  } else {
    sanitizedPri = pri.round(marketInfo.priceIncrement, MonetaryAmount::RoundType::kNearest);
  }
  if (sanitizedPri != pri) {
    log::debug("Sanitize price {} -> {}", pri.str(), sanitizedPri.str());
  }
  return sanitizedPri;
}

MonetaryAmount KucoinPublic::sanitizeVolume(Market m, MonetaryAmount vol) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(m)->second;
  MonetaryAmount sanitizedVol = vol;
  // TODO: Kucoin documentation is not clear about this, this would probably need to be adjusted
  if (sanitizedVol < marketInfo.baseMinSize) {
    sanitizedVol = marketInfo.baseMinSize;
  } else if (sanitizedVol > marketInfo.baseMaxSize) {
    sanitizedVol = marketInfo.baseMaxSize;
  } else {
    sanitizedVol = sanitizedVol.round(marketInfo.baseIncrement, MonetaryAmount::RoundType::kDown);
  }
  if (sanitizedVol != vol) {
    log::debug("Sanitize volume {} -> {}", vol.str(), sanitizedVol.str());
  }
  return sanitizedVol;
}

MonetaryAmount KucoinPublic::TradedVolumeFunc::operator()(Market m) {
  json result = PublicQuery(_curlHandle, "api/v1/market/stats", {{"symbol", m.assetsPairStr('-')}});
  return MonetaryAmount(result["vol"].get<std::string_view>(), m.base());
}

KucoinPublic::LastTradesVector KucoinPublic::queryLastTrades(Market m, int) {
  json result = PublicQuery(_curlHandle, "api/v1/market/histories", {{"symbol", m.assetsPairStr('-')}});
  LastTradesVector ret;
  ret.reserve(static_cast<LastTradesVector::size_type>(result.size()));
  for (const json& detail : result) {
    MonetaryAmount amount(detail["size"].get<std::string_view>(), m.base());
    MonetaryAmount price(detail["price"].get<std::string_view>(), m.quote());
    // time is in nanoseconds
    int64_t millisecondsSinceEpoch = static_cast<int64_t>(detail["time"].get<uintmax_t>() / 1000000UL);
    PublicTrade::Type tradeType =
        detail["side"].get<std::string_view>() == "buy" ? PublicTrade::Type::kBuy : PublicTrade::Type::kSell;

    ret.emplace_back(tradeType, amount, price,
                     PublicTrade::TimePoint(std::chrono::milliseconds(millisecondsSinceEpoch)));
  }
  std::sort(ret.begin(), ret.end());
  return ret;
}

MonetaryAmount KucoinPublic::TickerFunc::operator()(Market m) {
  json result = PublicQuery(_curlHandle, "api/v1/market/orderbook/level1", {{"symbol", m.assetsPairStr('-')}});
  return MonetaryAmount(result["price"].get<std::string_view>(), m.quote());
}
}  // namespace api
}  // namespace cct
