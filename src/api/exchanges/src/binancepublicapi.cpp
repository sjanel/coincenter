#include "binancepublicapi.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <execution>
#include <thread>
#include <unordered_map>

#include "apikey.hpp"
#include "cct_codec.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "jsonhelpers.hpp"
#include "monetaryamount.hpp"
#include "ssl_sha.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view baseURL, std::string_view method,
                 const CurlPostData& curlPostData = CurlPostData()) {
  std::string url(baseURL);
  url.append("/api/v3/");
  url.append(method);
  if (!curlPostData.empty()) {
    url.push_back('?');
    url.append(curlPostData.toStringView());
  }
  CurlOptions opts(CurlOptions::RequestType::kGet);
  opts.userAgent = BinancePublic::kUserAgent;
  json dataJson = json::parse(curlHandle.query(url, opts));
  if (dataJson.contains("code") && dataJson.contains("msg")) {
    const int statusCode = dataJson["code"];  // "1100" for instance
    const std::string_view errorMessage = dataJson["msg"].get<std::string_view>();
    throw exception("error " + std::to_string(statusCode) + ", msg: " + std::string(errorMessage));
  }
  return dataJson;
}

CurlHandle::Clock::duration Ping(CurlHandle& curlHandle, std::string_view baseURL) {
  CurlHandle::TimePoint t1 = CurlHandle::Clock::now();
  json sysTime = PublicQuery(curlHandle, baseURL, "time");
  CurlHandle::TimePoint t2 = CurlHandle::Clock::now();
  CurlHandle::Clock::duration ping = t2 - t1;
  if (!sysTime.contains("serverTime")) {
    throw exception("Binance reply should contain system time information");
  }
  CurlHandle::TimePoint binanceSysTimePoint(std::chrono::milliseconds(sysTime["serverTime"].get<uint64_t>()));
  if (t1 < binanceSysTimePoint && binanceSysTimePoint < t2) {
    log::debug("Your system clock is synchronized with Binance one");
  } else {
    log::error("Your system clock is not synchronized with Binance one");
    log::error("You may experience issues with the recvWindow while discussing with Binance");
  }
  log::debug("Binance base URL {} ping is {} ms", baseURL,
             std::chrono::duration_cast<std::chrono::milliseconds>(ping).count());
  return ping;
}

template <class ExchangeInfoDataByMarket>
const json& RetrieveMarketData(const ExchangeInfoDataByMarket& exchangeInfoData, Market m) {
  auto it = exchangeInfoData.find(m);
  if (it == exchangeInfoData.end()) {
    throw exception("Unable to retrieve market data " + m.str());
  }
  return it->second;
}

template <class ExchangeInfoDataByMarket>
VolAndPriNbDecimals QueryVolAndPriNbDecimals(const ExchangeInfoDataByMarket& exchangeInfoData, Market m) {
  const json& marketData = RetrieveMarketData(exchangeInfoData, m);
  return VolAndPriNbDecimals(marketData["baseAssetPrecision"].get<int8_t>(),
                             marketData["quoteAssetPrecision"].get<int8_t>());
}
}  // namespace

BinancePublic::BinancePublic(CoincenterInfo& config, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("binance", fiatConverter, cryptowatchAPI, config),
      _commonInfo(config.exchangeInfo(_name), config.getRunMode()),
      _exchangeInfoCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault), config,
          _commonInfo),
      _globalInfosCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kWithdrawalFees), _cachedResultVault)),
      _marketsCache(CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kMarkets), _cachedResultVault),
                    _exchangeInfoCache, _commonInfo._curlHandle, _commonInfo._exchangeInfo),
      _allOrderBooksCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAllOrderBooks), _cachedResultVault),
          _exchangeInfoCache, _marketsCache, _commonInfo),
      _orderbookCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kOrderBook), _cachedResultVault), config,
          _commonInfo),
      _tradedVolumeCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kTradedVolume), _cachedResultVault),
          _commonInfo) {}

BinancePublic::CommonInfo::CommonInfo(const ExchangeInfo& exchangeInfo, settings::RunMode runMode)
    : _exchangeInfo(exchangeInfo),
      _curlHandle(_exchangeInfo.minPublicQueryDelay(), runMode),
      _baseURLUpdater(CachedResultOptions(std::chrono::hours(96))) {}

std::string_view BinancePublic::CommonInfo::BaseURLUpdater::operator()() {
  CurlHandle::Clock::duration pingDurations[kNbBaseURLs];
  std::transform(std::execution::par, std::begin(_curlHandles), std::end(_curlHandles),
                 std::begin(BinancePublic::kURLBases), std::begin(pingDurations), Ping);
  int minPingDurationPos = static_cast<int>(std::min_element(std::begin(pingDurations), std::end(pingDurations)) -
                                            std::begin(pingDurations));
  std::string_view fastestBaseUrl = BinancePublic::kURLBases[minPingDurationPos];
  log::info("Selecting Binance base URL {} (ping of {} ms)", fastestBaseUrl,
            std::chrono::duration_cast<std::chrono::milliseconds>(pingDurations[minPingDurationPos]).count());
  return fastestBaseUrl;
}

CurrencyExchangeFlatSet BinancePublic::queryTradableCurrencies() {
  CurrencyExchangeFlatSet ret;
  const ExchangeInfo::CurrencySet& excludedCurrencies = _commonInfo._exchangeInfo.excludedCurrenciesAll();
  for (const json& el : _globalInfosCache.get()) {
    std::string_view coin = el["coin"].get<std::string_view>();
    if (coin.size() > CurrencyCode::kAcronymMaxLen) {
      continue;
    }
    CurrencyCode cur(coin);
    if (excludedCurrencies.contains(cur)) {
      log::trace("Discard {} excluded by config", cur.str());
      continue;
    }
    const json& networkListPart = el["networkList"].front();
    bool withdrawEnable = networkListPart["withdrawEnable"];
    bool depositEnable = networkListPart["depositEnable"];
    ret.insert(CurrencyExchange(
        cur, cur, cur, depositEnable ? CurrencyExchange::Deposit::kAvailable : CurrencyExchange::Deposit::kUnavailable,
        withdrawEnable ? CurrencyExchange::Withdraw::kAvailable : CurrencyExchange::Withdraw::kUnavailable));
  }

  log::info("Retrieved {} {} currencies", _name, ret.size());
  return ret;
}

ExchangePublic::MarketSet BinancePublic::MarketsFunc::operator()() {
  BinancePublic::ExchangeInfoFunc::ExchangeInfoDataByMarket exchangeInfoData = _exchangeInfoCache.get();
  const ExchangeInfo::CurrencySet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  MarketSet ret;
  ret.reserve(static_cast<MarketSet::size_type>(exchangeInfoData.size()));
  for (auto it = exchangeInfoData.begin(), endIt = exchangeInfoData.end(); it != endIt; ++it) {
    const json& symbol = it->second;
    std::string_view baseAsset = symbol["baseAsset"].get<std::string_view>();
    std::string_view quoteAsset = symbol["quoteAsset"].get<std::string_view>();
    CurrencyCode base(baseAsset);
    CurrencyCode quote(quoteAsset);
    if (excludedCurrencies.contains(base) || excludedCurrencies.contains(quote)) {
      continue;
    }
    ret.insert(Market(base, quote));
  }
  log::info("Retrieved binance {} markets", ret.size());
  return ret;
}

BinancePublic::ExchangeInfoFunc::ExchangeInfoDataByMarket BinancePublic::ExchangeInfoFunc::operator()() {
  ExchangeInfoDataByMarket ret;
  json exchangeInfoData = PublicQuery(_commonInfo._curlHandle, _commonInfo.getBestBaseURL(), "exchangeInfo");
  json& symbols = exchangeInfoData["symbols"];
  for (auto it = std::make_move_iterator(symbols.begin()), endIt = std::make_move_iterator(symbols.end()); it != endIt;
       ++it) {
    std::string_view baseAsset = (*it)["baseAsset"].get<std::string_view>();
    std::string_view quoteAsset = (*it)["quoteAsset"].get<std::string_view>();
    if ((*it)["status"].get<std::string_view>() != "TRADING") {
      log::trace("Discard {}-{} as not trading status {}", baseAsset, quoteAsset,
                 (*it)["status"].get<std::string_view>());
      continue;
    }
    if ((*it)["permissions"].size() == 1 && (*it)["permissions"].front().get<std::string_view>() == "LEVERAGED") {
      // These are '*DOWN' and '*UP' assets, do not take them into account for now
      log::trace("Discard {}-{} as coincenter does not support leveraged markets", baseAsset, quoteAsset);
      continue;
    }
    if (baseAsset.size() > CurrencyCode::kAcronymMaxLen || quoteAsset.size() > CurrencyCode::kAcronymMaxLen) {
      log::trace("Discard {}-{} as one asset is too long", baseAsset, quoteAsset);
      continue;
    }
    log::debug("Accept {}-{} Binance asset pair", baseAsset, quoteAsset);
    ret.insert_or_assign(Market(baseAsset, quoteAsset), std::move(*it));
  }
  return ret;
}

json BinancePublic::GlobalInfosFunc::operator()() {
  constexpr char kInfoFeeUrl[] = "https://www.binance.com/en/fee/depositFee";
  std::string s = _curlHandle.query(kInfoFeeUrl, CurlOptions(CurlOptions::RequestType::kGet));
  constexpr std::string_view appBegJson = "application/json\">";
  std::string::const_iterator first = s.begin() + s.find(appBegJson) + appBegJson.size();
  std::string::const_iterator last = s.begin() + s.find("}}</script><div", first - s.begin()) + 2;
  std::string_view jsonPart(first, last);
  // This json is HUGE and contains numerous amounts of information
  json globInfo = json::parse(jsonPart);
  for (const json& assetsInfo : globInfo) {
    if (assetsInfo.contains("redux")) {
      const json& assets = assetsInfo["redux"];
      for (const json& asset : assets) {
        if (asset.contains("depositFee")) {
          return asset["depositFee"];
        }
      }
    }
  }
  throw exception("Unexpected json from " + std::string(kInfoFeeUrl));
}

namespace {
MonetaryAmount ComputeWithdrawalFeesFromNetworkList(CurrencyCode cur, const json& networkList) {
  MonetaryAmount withdrawFee(0, cur, 0);
  for (const json& networkListPart : networkList) {
    withdrawFee = std::max(withdrawFee, MonetaryAmount(networkListPart["withdrawFee"].get<std::string_view>(), cur));
  }
  return withdrawFee;
}
}  // namespace

ExchangePublic::WithdrawalFeeMap BinancePublic::queryWithdrawalFees() {
  WithdrawalFeeMap ret;
  for (const json& el : _globalInfosCache.get()) {
    std::string_view coinStr = el["coin"].get<std::string_view>();
    if (coinStr.size() > CurrencyCode::kAcronymMaxLen) {
      continue;
    }
    CurrencyCode cur(coinStr);
    MonetaryAmount withdrawFee = ComputeWithdrawalFeesFromNetworkList(cur, el["networkList"]);
    log::trace("Retrieved {} withdrawal fee {}", _name, withdrawFee.str());
    ret.insert_or_assign(cur, withdrawFee);
  }

  log::info("Retrieved {} withdrawal fees for {} coins", _name, ret.size());
  assert(!ret.empty());
  return ret;
}

MonetaryAmount BinancePublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  for (const json& el : _globalInfosCache.get()) {
    CurrencyCode cur(el["coin"].get<std::string_view>());
    if (cur == currencyCode) {
      return ComputeWithdrawalFeesFromNetworkList(cur, el["networkList"]);
    }
  }
  throw exception("Unable to find withdrawal fee for " + std::string(currencyCode.str()));
}

MonetaryAmount BinancePublic::sanitizePrice(Market m, MonetaryAmount pri) {
  const json& marketData = RetrieveMarketData(_exchangeInfoCache.get(), m);

  const json* priceFilter = nullptr;
  MonetaryAmount ret(pri);
  for (const json& filter : marketData["filters"]) {
    const std::string_view filterType = filter["filterType"].get<std::string_view>();
    if (filterType == "PRICE_FILTER") {
      priceFilter = std::addressof(filter);
      break;
    }
  }

  if (priceFilter) {
    MonetaryAmount maxPrice((*priceFilter)["maxPrice"].get<std::string_view>(), ret.currencyCode());
    MonetaryAmount minPrice((*priceFilter)["minPrice"].get<std::string_view>(), ret.currencyCode());
    MonetaryAmount tickSize((*priceFilter)["tickSize"].get<std::string_view>(), ret.currencyCode());

    if (ret > maxPrice) {
      log::debug("Too big price {} capped to {} for {}", ret.str(), maxPrice.str(), m.str());
      ret = maxPrice;
    } else if (ret < minPrice) {
      log::debug("Too small price {} increased to {} for {}", ret.str(), minPrice.str(), m.str());
      ret = minPrice;
    } else {
      MonetaryAmount roundedPri = ret.round(tickSize, MonetaryAmount::RoundType::kDown);
      if (roundedPri != ret) {
        log::debug("Rounded {} into {} according to {}", ret.str(), roundedPri.str(), m.str());
      }
      ret = roundedPri;
    }
  }

  VolAndPriNbDecimals volAndPriNbDecimals = QueryVolAndPriNbDecimals(_exchangeInfoCache.get(), m);
  ret.truncate(volAndPriNbDecimals.priNbDecimals);
  if (pri != ret) {
    log::warn("Sanitize price {} -> {}", pri.str(), ret.str());
  }
  return ret;
}

MonetaryAmount BinancePublic::sanitizeVolume(Market m, MonetaryAmount vol, MonetaryAmount sanitizedPrice,
                                             bool isTakerOrder) {
  const json& marketData = RetrieveMarketData(_exchangeInfoCache.get(), m);
  MonetaryAmount ret(vol);

  const json* minNotionalFilter = nullptr;
  const json* lotSizeFilter = nullptr;
  const json* marketLotSizeFilter = nullptr;
  for (const json& filter : marketData["filters"]) {
    const std::string_view filterType = filter["filterType"].get<std::string_view>();
    if (filterType == "LOT_SIZE") {
      lotSizeFilter = std::addressof(filter);
    } else if (filterType == "MARKET_LOT_SIZE") {
      if (isTakerOrder) {
        marketLotSizeFilter = std::addressof(filter);
      }
    } else if (filterType == "MIN_NOTIONAL") {
      minNotionalFilter = std::addressof(filter);
    }
  }

  MonetaryAmount minVolumeAfterMinNotional("0", ret.currencyCode());
  if (minNotionalFilter) {
    // "applyToMarket": true,
    // "avgPriceMins": 5,
    // "minNotional": "0.05000000"
    MonetaryAmount minNotional((*minNotionalFilter)["minNotional"].get<std::string_view>());
    MonetaryAmount priceTimesQuantity = ret.toNeutral() * sanitizedPrice.toNeutral();
    minVolumeAfterMinNotional = MonetaryAmount(minNotional / sanitizedPrice, ret.currencyCode());
    if (priceTimesQuantity < minNotional) {
      log::debug("Too small min price * quantity. {} increased to {} for {}", ret.str(),
                 minVolumeAfterMinNotional.str(), m.str());
      ret = minVolumeAfterMinNotional;
    }
  }

  for (const json* lotFilterPtr : {marketLotSizeFilter, lotSizeFilter}) {
    if (lotFilterPtr) {
      // "maxQty": "9000000.00000000",
      // "minQty": "1.00000000",
      // "stepSize": "1.00000000"
      MonetaryAmount maxQty((*lotFilterPtr)["maxQty"].get<std::string_view>(), ret.currencyCode());
      MonetaryAmount minQty((*lotFilterPtr)["minQty"].get<std::string_view>(), ret.currencyCode());
      MonetaryAmount stepSize((*lotFilterPtr)["stepSize"].get<std::string_view>(), ret.currencyCode());

      if (ret > maxQty) {
        log::debug("Too big volume {} capped to {} for {}", ret.str(), maxQty.str(), m.str());
        ret = maxQty;
      } else if (ret < minQty) {
        log::debug("Too small volume {} increased to {} for {}", ret.str(), minQty.str(), m.str());
        ret = minQty;
      } else if (!stepSize.isZero()) {
        MonetaryAmount roundedVol = ret.round(stepSize, MonetaryAmount::RoundType::kDown);
        if (roundedVol < minVolumeAfterMinNotional) {
          roundedVol = ret.round(stepSize, MonetaryAmount::RoundType::kUp);
        }
        if (roundedVol != ret) {
          log::debug("Rounded {} into {} according to {}", ret.str(), roundedVol.str(), m.str());
        }
        ret = roundedVol;
      }
    }
  }

  VolAndPriNbDecimals volAndPriNbDecimals = QueryVolAndPriNbDecimals(_exchangeInfoCache.get(), m);
  ret.truncate(volAndPriNbDecimals.volNbDecimals);
  if (ret != vol) {
    log::warn("Sanitize volume {} -> {}", vol.str(), ret.str());
  }
  return ret;
}

ExchangePublic::MarketOrderBookMap BinancePublic::AllOrderBooksFunc::operator()(int depth) {
  MarketOrderBookMap ret;
  const MarketSet& markets = _marketsCache.get();
  json result = PublicQuery(_commonInfo._curlHandle, _commonInfo.getBestBaseURL(), "ticker/bookTicker");
  using BinanceAssetPairToStdMarketMap = std::unordered_map<std::string, Market>;
  BinanceAssetPairToStdMarketMap binanceAssetPairToStdMarketMap;
  binanceAssetPairToStdMarketMap.reserve(markets.size());
  for (Market m : markets) {
    binanceAssetPairToStdMarketMap.insert_or_assign(m.assetsPairStr(), m);
  }
  for (const json& tickerDetails : result) {
    std::string assetsPairStr = tickerDetails["symbol"];
    auto it = binanceAssetPairToStdMarketMap.find(assetsPairStr);
    if (it == binanceAssetPairToStdMarketMap.end()) {
      continue;
    }
    Market m = it->second;
    MonetaryAmount askPri(tickerDetails["askPrice"].get<std::string_view>(), m.quote());
    MonetaryAmount bidPri(tickerDetails["bidPrice"].get<std::string_view>(), m.quote());
    MonetaryAmount askVol(tickerDetails["askQty"].get<std::string_view>(), m.base());
    MonetaryAmount bidVol(tickerDetails["bidQty"].get<std::string_view>(), m.base());

    ret.insert_or_assign(m, MarketOrderBook(askPri, askVol, bidPri, bidVol,
                                            QueryVolAndPriNbDecimals(_exchangeInfoCache.get(), m), depth));
  }

  log::info("Retrieved ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook BinancePublic::OrderBookFunc::operator()(Market m, int depth) {
  // Binance has a fixed range of authorized values for depth
  constexpr int kAuthorizedDepths[] = {5, 10, 20, 50, 100, 500, 1000, 5000};
  auto lb = std::lower_bound(std::begin(kAuthorizedDepths), std::end(kAuthorizedDepths), depth);
  if (lb == std::end(kAuthorizedDepths)) {
    lb = std::next(std::end(kAuthorizedDepths), -1);
    log::error("Invalid depth {}, default to {}", *lb);
  }
  CurlPostData postData{{"symbol", m.assetsPairStr()}, {"limit", *lb}};
  json asksAndBids = PublicQuery(_commonInfo._curlHandle, _commonInfo.getBestBaseURL(), "depth", postData);
  const json& asks = asksAndBids["asks"];
  const json& bids = asksAndBids["bids"];
  using OrderBookVec = cct::vector<OrderBookLine>;
  OrderBookVec orderBookLines;
  orderBookLines.reserve(static_cast<OrderBookVec::size_type>(asks.size() + bids.size()));
  for (auto asksOrBids : {std::addressof(asks), std::addressof(bids)}) {
    const bool isAsk = asksOrBids == std::addressof(asks);
    for (const auto& priceQuantityPair : *asksOrBids) {
      MonetaryAmount amount(priceQuantityPair.back().get<std::string_view>(), m.base());
      MonetaryAmount price(priceQuantityPair.front().get<std::string_view>(), m.quote());

      orderBookLines.emplace_back(amount, price, isAsk);
    }
  }
  return MarketOrderBook(m, orderBookLines);
}

MonetaryAmount BinancePublic::TradedVolumeFunc::operator()(Market m) {
  json result = PublicQuery(_commonInfo._curlHandle, _commonInfo.getBestBaseURL(), "ticker/24hr",
                            {{"symbol", m.assetsPairStr()}});
  std::string_view last24hVol = result["volume"].get<std::string_view>();
  return MonetaryAmount(last24hVol, m.base());
}

}  // namespace api
}  // namespace cct
