#include "binancepublicapi.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "binance-common-api.hpp"
#include "cachedresult.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "httprequesttype.hpp"
#include "market-vector.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "order-book-line.hpp"
#include "permanentcurloptions.hpp"
#include "public-trade-vector.hpp"
#include "request-retry.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct::api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view method, const CurlPostData& curlPostData = CurlPostData()) {
  string endpoint(method);
  if (!curlPostData.empty()) {
    endpoint.push_back('?');
    endpoint.append(curlPostData.str());
  }
  RequestRetry requestRetry(curlHandle, CurlOptions(HttpRequestType::kGet));

  return requestRetry.queryJson(endpoint, [](const json& jsonResponse) {
    const auto foundErrorIt = jsonResponse.find("code");
    const auto foundMsgIt = jsonResponse.find("msg");
    if (foundErrorIt != jsonResponse.end() && foundMsgIt != jsonResponse.end()) {
      const int statusCode = foundErrorIt->get<int>();  // "1100" for instance
      log::warn("Binance error ({}), full json: '{}'", statusCode, jsonResponse.dump());
      return RequestRetry::Status::kResponseError;
    }
    return RequestRetry::Status::kResponseOK;
  });
}

template <class ExchangeInfoDataByMarket>
const json& RetrieveMarketData(const ExchangeInfoDataByMarket& exchangeInfoData, Market mk) {
  auto it = exchangeInfoData.find(mk);
  if (it == exchangeInfoData.end()) {
    throw exception("Unable to retrieve {} data", mk);
  }
  return it->second;
}

template <class ExchangeInfoDataByMarket>
VolAndPriNbDecimals QueryVolAndPriNbDecimals(const ExchangeInfoDataByMarket& exchangeInfoData, Market mk) {
  const json& marketData = RetrieveMarketData(exchangeInfoData, mk);
  return {marketData["baseAssetPrecision"].get<int8_t>(), marketData["quoteAssetPrecision"].get<int8_t>()};
}
}  // namespace

BinancePublic::BinancePublic(const CoincenterInfo& coincenterInfo, FiatConverter& fiatConverter,
                             api::CommonAPI& commonAPI)
    : ExchangePublic("binance", fiatConverter, commonAPI, coincenterInfo),
      _commonInfo(coincenterInfo, exchangeConfig(), coincenterInfo.getRunMode()),
      _exchangeConfigCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault),
          _commonInfo),
      _marketsCache(CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kMarkets), _cachedResultVault),
                    _exchangeConfigCache, _commonInfo._curlHandle, _commonInfo._exchangeConfig),
      _allOrderBooksCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kAllOrderBooks), _cachedResultVault),
          _exchangeConfigCache, _marketsCache, _commonInfo),
      _orderbookCache(CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kOrderBook), _cachedResultVault),
                      _commonInfo),
      _tradedVolumeCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kTradedVolume), _cachedResultVault),
          _commonInfo),
      _tickerCache(CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kLastPrice), _cachedResultVault),
                   _commonInfo) {}

bool BinancePublic::healthCheck() {
  static constexpr bool kAllowExceptions = false;
  json result = json::parse(_commonInfo._curlHandle.query("/api/v3/ping", CurlOptions(HttpRequestType::kGet)), nullptr,
                            kAllowExceptions);
  if (result.is_discarded()) {
    log::error("{} health check response is badly formatted: {}", _name, result.dump());
    return false;
  }
  if (!result.empty()) {
    log::error("{} health check is not empty: {}", _name, result.dump());
  }
  return result.empty();
}

CurrencyExchangeFlatSet BinancePublic::queryTradableCurrencies() {
  return commonAPI().getBinanceGlobalInfos().queryTradableCurrencies(_exchangeConfig.excludedCurrenciesAll());
}

MonetaryAmountByCurrencySet BinancePublic::queryWithdrawalFees() {
  return commonAPI().getBinanceGlobalInfos().queryWithdrawalFees();
}

std::optional<MonetaryAmount> BinancePublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  MonetaryAmount withdrawFee = commonAPI().getBinanceGlobalInfos().queryWithdrawalFee(currencyCode);
  if (withdrawFee.isDefault()) {
    return {};
  }
  return withdrawFee;
}

BinancePublic::CommonInfo::CommonInfo(const CoincenterInfo& coincenterInfo, const ExchangeConfig& exchangeConfig,
                                      settings::RunMode runMode)
    : _exchangeConfig(exchangeConfig),
      _curlHandle(kURLBases, coincenterInfo.metricGatewayPtr(),
                  _exchangeConfig.curlOptionsBuilderBase(ExchangeConfig::Api::kPublic).build(), runMode) {}

MarketSet BinancePublic::MarketsFunc::operator()() {
  BinancePublic::ExchangeInfoFunc::ExchangeInfoDataByMarket exchangeInfoData = _exchangeConfigCache.get();
  const CurrencyCodeSet& excludedCurrencies = _exchangeConfig.excludedCurrenciesAll();

  MarketVector markets;
  markets.reserve(static_cast<MarketSet::size_type>(exchangeInfoData.size()));

  for (const auto& marketJsonPair : exchangeInfoData) {
    const json& symbol = marketJsonPair.second;
    std::string_view baseAsset = symbol["baseAsset"].get<std::string_view>();
    std::string_view quoteAsset = symbol["quoteAsset"].get<std::string_view>();
    CurrencyCode base(baseAsset);
    CurrencyCode quote(quoteAsset);
    if (excludedCurrencies.contains(base) || excludedCurrencies.contains(quote)) {
      continue;
    }
    markets.emplace_back(base, quote);
  }
  MarketSet ret(std::move(markets));
  log::debug("Retrieved {} markets from binance", ret.size());
  return ret;
}

BinancePublic::ExchangeInfoFunc::ExchangeInfoDataByMarket BinancePublic::ExchangeInfoFunc::operator()() {
  ExchangeInfoDataByMarket ret;
  json exchangeInfoData = PublicQuery(_commonInfo._curlHandle, "/api/v3/exchangeInfo");
  auto symbolsIt = exchangeInfoData.find("symbols");
  if (symbolsIt == exchangeInfoData.end()) {
    return ret;
  }
  for (auto it = std::make_move_iterator(symbolsIt->begin()), endIt = std::make_move_iterator(symbolsIt->end());
       it != endIt; ++it) {
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
    if (baseAsset.size() > CurrencyCode::kMaxLen || quoteAsset.size() > CurrencyCode::kMaxLen) {
      log::trace("Discard {}-{} as one asset is too long", baseAsset, quoteAsset);
      continue;
    }
    log::trace("Accept {}-{} Binance asset pair", baseAsset, quoteAsset);
    ret.insert_or_assign(Market(baseAsset, quoteAsset), std::move(*it));
  }
  return ret;
}

MonetaryAmount BinancePublic::sanitizePrice(Market mk, MonetaryAmount pri) {
  const json& marketData = RetrieveMarketData(_exchangeConfigCache.get(), mk);

  const json* pPriceFilter = nullptr;
  MonetaryAmount ret(pri);
  for (const json& filter : marketData["filters"]) {
    const std::string_view filterType = filter["filterType"].get<std::string_view>();
    if (filterType == "PRICE_FILTER") {
      pPriceFilter = std::addressof(filter);
      break;
    }
  }

  if (pPriceFilter != nullptr) {
    MonetaryAmount maxPrice((*pPriceFilter)["maxPrice"].get<std::string_view>(), ret.currencyCode());
    MonetaryAmount minPrice((*pPriceFilter)["minPrice"].get<std::string_view>(), ret.currencyCode());
    MonetaryAmount tickSize((*pPriceFilter)["tickSize"].get<std::string_view>(), ret.currencyCode());

    if (ret > maxPrice) {
      log::debug("Too big price {} capped to {} for {}", ret, maxPrice, mk);
      ret = maxPrice;
    } else if (ret < minPrice) {
      log::debug("Too small price {} increased to {} for {}", ret, minPrice, mk);
      ret = minPrice;
    } else {
      ret.round(tickSize, MonetaryAmount::RoundType::kDown);
      if (ret != pri) {
        log::debug("Rounded {} into {} according to {}", pri, ret, mk);
      }
    }
  }

  VolAndPriNbDecimals volAndPriNbDecimals = QueryVolAndPriNbDecimals(_exchangeConfigCache.get(), mk);
  ret.truncate(volAndPriNbDecimals.priNbDecimals);
  if (pri != ret) {
    log::warn("Sanitize price {} -> {}", pri, ret);
  }
  return ret;
}

MonetaryAmount BinancePublic::computePriceForNotional(Market mk, int avgPriceMins) {
  if (avgPriceMins == 0) {
    // price should be the last matched price
    PublicTradeVector lastTrades = getLastTrades(mk, 1);
    if (!lastTrades.empty()) {
      return lastTrades.front().price();
    }
    log::error("Unable to retrieve last trades from {}, use average price instead for notional", mk);
  }

  const json result = PublicQuery(_commonInfo._curlHandle, "/api/v3/avgPrice", {{"symbol", mk.assetsPairStrUpper()}});
  const auto priceIt = result.find("price");
  const std::string_view priceStr = priceIt == result.end() ? std::string_view() : priceIt->get<std::string_view>();

  return {priceStr, mk.quote()};
}

MonetaryAmount BinancePublic::sanitizeVolume(Market mk, MonetaryAmount vol, MonetaryAmount priceForNotional,
                                             bool isTakerOrder) {
  const json& marketData = RetrieveMarketData(_exchangeConfigCache.get(), mk);
  MonetaryAmount ret(vol);

  const json* pMinNotionalFilter = nullptr;
  const json* pNotionalFilter = nullptr;
  const json* pLotSizeFilter = nullptr;
  const json* pMarketLotSizeFilter = nullptr;

  for (const json& filter : marketData["filters"]) {
    const std::string_view filterType = filter["filterType"].get<std::string_view>();
    if (filterType == "LOT_SIZE") {
      pLotSizeFilter = std::addressof(filter);
    } else if (filterType == "MARKET_LOT_SIZE") {
      if (isTakerOrder) {
        pMarketLotSizeFilter = std::addressof(filter);
      }
    } else if (filterType == "MIN_NOTIONAL") {
      if (isTakerOrder) {
        if (filter["applyToMarket"].get<bool>()) {
          priceForNotional = computePriceForNotional(mk, filter["avgPriceMins"].get<int>());
          pMinNotionalFilter = std::addressof(filter);
        }
      } else {
        pMinNotionalFilter = std::addressof(filter);
      }
    } else if (filterType == "NOTIONAL") {
      if (isTakerOrder) {
        if (filter["applyMinToMarket"].get<bool>() || filter["applyMaxToMarket"].get<bool>()) {
          priceForNotional = computePriceForNotional(mk, filter["avgPriceMins"].get<int>());
          pNotionalFilter = std::addressof(filter);
        }
      } else {
        pNotionalFilter = std::addressof(filter);
      }
    }
  }

  MonetaryAmount minVolumeAfterMinNotional(0, ret.currencyCode());
  if (pMinNotionalFilter != nullptr) {
    MonetaryAmount minNotional((*pMinNotionalFilter)["minNotional"].get<std::string_view>());
    MonetaryAmount priceTimesQuantity = ret.toNeutral() * priceForNotional.toNeutral();

    minVolumeAfterMinNotional = MonetaryAmount(minNotional / priceForNotional, ret.currencyCode());
    if (priceTimesQuantity < minNotional) {
      log::debug("Too small min price * quantity. {} increased to {} for {}", ret, minVolumeAfterMinNotional, mk);
      ret = minVolumeAfterMinNotional;
    }
  }

  if (pNotionalFilter != nullptr) {
    MonetaryAmount priceTimesQuantity = ret.toNeutral() * priceForNotional.toNeutral();

    if (!isTakerOrder || (*pNotionalFilter)["applyMinToMarket"].get<bool>()) {
      // min notional applies
      MonetaryAmount minNotional((*pNotionalFilter)["minNotional"].get<std::string_view>());

      minVolumeAfterMinNotional =
          std::max(minVolumeAfterMinNotional, MonetaryAmount(minNotional / priceForNotional, ret.currencyCode()));

      if (priceTimesQuantity < minNotional) {
        log::debug("Too small (price * quantity). {} increased to {} for {}", ret, minVolumeAfterMinNotional, mk);
        ret = minVolumeAfterMinNotional;
      }
    } else if (!isTakerOrder || (*pNotionalFilter)["applyMaxToMarket"].get<bool>()) {
      // max notional applies
      MonetaryAmount maxNotional((*pNotionalFilter)["maxNotional"].get<std::string_view>());
      MonetaryAmount maxVolumeAfterMaxNotional = MonetaryAmount(maxNotional / priceForNotional, ret.currencyCode());

      if (priceTimesQuantity > maxNotional) {
        log::debug("Too large (price * quantity). {} decreased to {} for {}", ret, maxVolumeAfterMaxNotional, mk);
        ret = maxVolumeAfterMaxNotional;
      }
    }
  }

  for (const json* pLotFilterPtr : {pMarketLotSizeFilter, pLotSizeFilter}) {
    if (pLotFilterPtr != nullptr) {
      // "maxQty": "9000000.00000000",
      // "minQty": "1.00000000",
      // "stepSize": "1.00000000"
      MonetaryAmount maxQty((*pLotFilterPtr)["maxQty"].get<std::string_view>(), ret.currencyCode());
      MonetaryAmount minQty((*pLotFilterPtr)["minQty"].get<std::string_view>(), ret.currencyCode());
      MonetaryAmount stepSize((*pLotFilterPtr)["stepSize"].get<std::string_view>(), ret.currencyCode());

      if (ret > maxQty) {
        log::debug("Too big volume {} capped to {} for {}", ret, maxQty, mk);
        ret = maxQty;
      } else if (ret < minQty) {
        log::debug("Too small volume {} increased to {} for {}", ret, minQty, mk);
        ret = minQty;
      } else if (stepSize != 0) {
        if (ret == minVolumeAfterMinNotional) {
          ret.round(stepSize, MonetaryAmount::RoundType::kUp);
          log::debug("{} rounded up to {} because {} min notional applied", minVolumeAfterMinNotional, ret, mk);
        } else {
          ret.round(stepSize, MonetaryAmount::RoundType::kDown);
          log::debug("{} rounded down to {} according to {}", vol, ret, mk);
        }
      }
    }
  }

  VolAndPriNbDecimals volAndPriNbDecimals = QueryVolAndPriNbDecimals(_exchangeConfigCache.get(), mk);
  ret.truncate(volAndPriNbDecimals.volNbDecimals);
  if (ret != vol) {
    log::warn("Sanitize volume {} -> {}", vol, ret);
  }
  return ret;
}

MarketOrderBookMap BinancePublic::AllOrderBooksFunc::operator()(int depth) {
  MarketOrderBookMap ret;
  const MarketSet& markets = _marketsCache.get();
  json result = PublicQuery(_commonInfo._curlHandle, "/api/v3/ticker/bookTicker");
  using BinanceAssetPairToStdMarketMap = std::unordered_map<string, Market>;
  BinanceAssetPairToStdMarketMap binanceAssetPairToStdMarketMap;
  binanceAssetPairToStdMarketMap.reserve(markets.size());
  for (Market mk : markets) {
    binanceAssetPairToStdMarketMap.insert_or_assign(mk.assetsPairStrUpper(), mk);
  }
  const auto time = Clock::now();
  for (const json& tickerDetails : result) {
    string assetsPairStr = tickerDetails["symbol"];
    auto it = binanceAssetPairToStdMarketMap.find(assetsPairStr);
    if (it == binanceAssetPairToStdMarketMap.end()) {
      continue;
    }
    Market mk = it->second;
    MonetaryAmount askPri(tickerDetails["askPrice"].get<std::string_view>(), mk.quote());
    MonetaryAmount bidPri(tickerDetails["bidPrice"].get<std::string_view>(), mk.quote());
    MonetaryAmount askVol(tickerDetails["askQty"].get<std::string_view>(), mk.base());
    MonetaryAmount bidVol(tickerDetails["bidQty"].get<std::string_view>(), mk.base());

    ret.insert_or_assign(mk, MarketOrderBook(time, askPri, askVol, bidPri, bidVol,
                                             QueryVolAndPriNbDecimals(_exchangeConfigCache.get(), mk), depth));
  }

  log::info("Retrieved ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook BinancePublic::OrderBookFunc::operator()(Market mk, int depth) {
  // Binance has a fixed range of authorized values for depth
  static constexpr std::array kAuthorizedDepths = {5, 10, 20, 50, 100, 500, 1000, 5000};
  auto lb = std::ranges::lower_bound(kAuthorizedDepths, depth);
  if (lb == kAuthorizedDepths.end()) {
    lb = std::next(kAuthorizedDepths.end(), -1);
    log::error("Invalid depth {}, default to {}", depth, *lb);
  }

  MarketOrderBookLines orderBookLines;

  const CurlPostData postData{{"symbol", mk.assetsPairStrUpper()}, {"limit", *lb}};
  const json asksAndBids = PublicQuery(_commonInfo._curlHandle, "/api/v3/depth", postData);
  const auto nowTime = Clock::now();
  const auto asksIt = asksAndBids.find("asks");
  const auto bidsIt = asksAndBids.find("bids");

  if (asksIt != asksAndBids.end() && bidsIt != asksAndBids.end()) {
    orderBookLines.reserve(std::min(static_cast<decltype(depth)>(asksIt->size()), depth) +
                           std::min(static_cast<decltype(depth)>(bidsIt->size()), depth));
    for (const auto& asksOrBids : {asksIt, bidsIt}) {
      const auto type = asksOrBids == asksIt ? OrderBookLine::Type::kAsk : OrderBookLine::Type::kBid;
      for (const auto& priceQuantityPair : *asksOrBids | std::ranges::views::take(depth)) {
        MonetaryAmount amount(priceQuantityPair.back().get<std::string_view>(), mk.base());
        MonetaryAmount price(priceQuantityPair.front().get<std::string_view>(), mk.quote());

        orderBookLines.push(amount, price, type);
      }
    }
  }

  return MarketOrderBook(nowTime, mk, orderBookLines);
}

MonetaryAmount BinancePublic::TradedVolumeFunc::operator()(Market mk) {
  const json result =
      PublicQuery(_commonInfo._curlHandle, "/api/v3/ticker/24hr", {{"symbol", mk.assetsPairStrUpper()}});
  const auto volumeIt = result.find("volume");
  const std::string_view last24hVol = volumeIt == result.end() ? std::string_view() : volumeIt->get<std::string_view>();

  return {last24hVol, mk.base()};
}

PublicTradeVector BinancePublic::queryLastTrades(Market mk, int nbTrades) {
  static constexpr int kMaxNbLastTrades = 1000;

  if (nbTrades > kMaxNbLastTrades) {
    log::warn("{} is larger than maximum number of last trades of {} on {}", nbTrades, kMaxNbLastTrades, _name);
    nbTrades = kMaxNbLastTrades;
  }

  json result = PublicQuery(_commonInfo._curlHandle, "/api/v3/trades",
                            {{"symbol", mk.assetsPairStrUpper()}, {"limit", nbTrades}});

  PublicTradeVector ret;
  ret.reserve(static_cast<PublicTradeVector::size_type>(result.size()));

  for (const json& detail : result) {
    MonetaryAmount amount(detail["qty"].get<std::string_view>(), mk.base());
    MonetaryAmount price(detail["price"].get<std::string_view>(), mk.quote());
    int64_t millisecondsSinceEpoch = detail["time"].get<int64_t>();
    TradeSide tradeSide = detail["isBuyerMaker"].get<bool>() ? TradeSide::kSell : TradeSide::kBuy;

    ret.emplace_back(tradeSide, amount, price, TimePoint(milliseconds(millisecondsSinceEpoch)));
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount BinancePublic::TickerFunc::operator()(Market mk) {
  const json data = PublicQuery(_commonInfo._curlHandle, "/api/v3/ticker/price", {{"symbol", mk.assetsPairStrUpper()}});
  const auto priceIt = data.find("price");
  const std::string_view lastPrice = priceIt == data.end() ? std::string_view() : priceIt->get<std::string_view>();
  return {lastPrice, mk.quote()};
}

}  // namespace cct::api
