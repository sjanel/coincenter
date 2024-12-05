#include "binancepublicapi.hpp"

#include <algorithm>
#include <amc/isdetected.hpp>
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
#include "binance-schema.hpp"
#include "cachedresult.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_json-serialization.hpp"
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
#include "timedef.hpp"
#include "tradeside.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct::api {
namespace {

template <class T>
T PublicQuery(CurlHandle& curlHandle, std::string_view method, const CurlPostData& curlPostData = CurlPostData()) {
  string endpoint(method);
  if (!curlPostData.empty()) {
    endpoint.push_back('?');
    endpoint.append(curlPostData.str());
  }
  RequestRetry requestRetry(curlHandle, CurlOptions(HttpRequestType::kGet));

  return requestRetry.query<T, json::opts{.error_on_unknown_keys = false, .minified = true, .raw_string = true}>(
      endpoint, [](const T& response) {
        if constexpr (amc::is_detected<has_code_t, T>::value && amc::is_detected<has_msg_t, T>::value) {
          if (response.code && response.msg) {
            const int statusCode = *response.code;  // "1100" for instance
            log::warn("Binance error ({}), msg: '{}'", statusCode, *response.msg);
            return RequestRetry::Status::kResponseError;
          }
        }

        return RequestRetry::Status::kResponseOK;
      });
}

const auto& RetrieveMarketData(const auto& exchangeInfoData, Market mk) {
  auto it = exchangeInfoData.find(mk);
  if (it == exchangeInfoData.end()) {
    throw exception("Unable to retrieve {} data", mk);
  }
  return it->second;
}

VolAndPriNbDecimals QueryVolAndPriNbDecimals(const auto& exchangeInfoData, Market mk) {
  const auto& marketData = RetrieveMarketData(exchangeInfoData, mk);
  return {marketData.baseAssetPrecision, marketData.quoteAssetPrecision};
}
}  // namespace

BinancePublic::BinancePublic(const CoincenterInfo& coincenterInfo, FiatConverter& fiatConverter,
                             api::CommonAPI& commonAPI)
    : ExchangePublic(ExchangeNameEnum::binance, fiatConverter, commonAPI, coincenterInfo),
      _curlHandle(kURLBases, coincenterInfo.metricGatewayPtr(), permanentCurlOptionsBuilder().build(),
                  coincenterInfo.getRunMode()),
      _commonInfo(exchangeConfig().asset, _curlHandle),
      _exchangeConfigCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::currencies).duration,
                              _cachedResultVault),
          _commonInfo),
      _marketsCache(CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::markets).duration,
                                        _cachedResultVault),
                    _exchangeConfigCache, _commonInfo._curlHandle, _commonInfo._assetConfig),
      _allOrderBooksCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::allOrderBooks).duration,
                              _cachedResultVault),
          _exchangeConfigCache, _marketsCache, _commonInfo),
      _orderbookCache(CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::orderBook).duration,
                                          _cachedResultVault),
                      _commonInfo),
      _tradedVolumeCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::tradedVolume).duration,
                              _cachedResultVault),
          _commonInfo),
      _tickerCache(CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::lastPrice).duration,
                                       _cachedResultVault),
                   _commonInfo) {}

bool BinancePublic::healthCheck() {
  auto result = _commonInfo._curlHandle.query("/api/v3/ping", CurlOptions(HttpRequestType::kGet));
  return result == "{}";
}

CurrencyExchangeFlatSet BinancePublic::queryTradableCurrencies() {
  return commonAPI().getBinanceGlobalInfos().queryTradableCurrencies(exchangeConfig().asset.allExclude);
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

MarketSet BinancePublic::MarketsFunc::operator()() {
  const auto& exchangeInfoData = _exchangeConfigCache.get();
  const CurrencyCodeSet& excludedCurrencies = _assetConfig.allExclude;

  MarketVector markets;
  markets.reserve(static_cast<MarketSet::size_type>(exchangeInfoData.size()));

  for (const auto& [mk, _] : exchangeInfoData) {
    if (excludedCurrencies.contains(mk.base()) || excludedCurrencies.contains(mk.quote())) {
      continue;
    }
    markets.push_back(mk);
  }
  MarketSet ret(std::move(markets));
  log::debug("Retrieved {} markets from binance", ret.size());
  return ret;
}

BinancePublic::ExchangeInfoFunc::ExchangeInfoDataByMarket BinancePublic::ExchangeInfoFunc::operator()() {
  ExchangeInfoDataByMarket ret;
  auto data = PublicQuery<schema::binance::V3ExchangeInfo>(_commonInfo._curlHandle, "/api/v3/exchangeInfo");
  for (auto& symbol : data.symbols) {
    if (symbol.status != "TRADING") {
      log::trace("Discard {}-{} as not trading status {}", symbol.baseAsset, symbol.quoteAsset, symbol.status);
      continue;
    }
    if (symbol.permissions.size() == 1 && symbol.permissions.front() == "LEVERAGED") {
      // These are '*DOWN' and '*UP' assets, do not take them into account for now
      log::trace("Discard {}-{} as coincenter does not support leveraged markets", symbol.baseAsset, symbol.quoteAsset);
      continue;
    }
    if (symbol.baseAsset.size() > CurrencyCode::kMaxLen || symbol.quoteAsset.size() > CurrencyCode::kMaxLen) {
      log::trace("Discard {}-{} as one asset is too long", symbol.baseAsset, symbol.quoteAsset);
      continue;
    }
    log::trace("Accept {}-{} Binance asset pair", symbol.baseAsset, symbol.quoteAsset);
    Market market(CurrencyCode{symbol.baseAsset}, CurrencyCode{symbol.quoteAsset});
    ret.insert_or_assign(std::move(market), std::move(symbol));
  }
  return ret;
}

MonetaryAmount BinancePublic::sanitizePrice(Market mk, MonetaryAmount pri) {
  const auto& exchangeConfigCache = _exchangeConfigCache.get();
  const auto& marketData = RetrieveMarketData(exchangeConfigCache, mk);

  const schema::binance::V3ExchangeInfo::Symbol::Filter* pPriceFilter = nullptr;
  MonetaryAmount ret(pri);
  for (const auto& filter : marketData.filters) {
    if (filter.filterType == "PRICE_FILTER") {
      pPriceFilter = std::addressof(filter);
      break;
    }
  }

  if (pPriceFilter != nullptr) {
    MonetaryAmount maxPrice(pPriceFilter->maxPrice, ret.currencyCode());
    MonetaryAmount minPrice(pPriceFilter->minPrice, ret.currencyCode());
    MonetaryAmount tickSize(pPriceFilter->tickSize, ret.currencyCode());

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

  VolAndPriNbDecimals volAndPriNbDecimals = QueryVolAndPriNbDecimals(exchangeConfigCache, mk);
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

  const auto result = PublicQuery<schema::binance::V3AvgPrice>(_commonInfo._curlHandle, "/api/v3/avgPrice",
                                                               {{"symbol", mk.assetsPairStrUpper()}});

  return {result.price, mk.quote()};
}

MonetaryAmount BinancePublic::sanitizeVolume(Market mk, MonetaryAmount vol, MonetaryAmount priceForNotional,
                                             bool isTakerOrder) {
  const auto& marketData = RetrieveMarketData(_exchangeConfigCache.get(), mk);
  MonetaryAmount ret(vol);

  const schema::binance::V3ExchangeInfo::Symbol::Filter* pMinNotionalFilter = nullptr;
  const schema::binance::V3ExchangeInfo::Symbol::Filter* pNotionalFilter = nullptr;
  const schema::binance::V3ExchangeInfo::Symbol::Filter* pLotSizeFilter = nullptr;
  const schema::binance::V3ExchangeInfo::Symbol::Filter* pMarketLotSizeFilter = nullptr;

  for (const auto& filter : marketData.filters) {
    if (filter.filterType == "LOT_SIZE") {
      pLotSizeFilter = std::addressof(filter);
    } else if (filter.filterType == "MARKET_LOT_SIZE") {
      if (isTakerOrder) {
        pMarketLotSizeFilter = std::addressof(filter);
      }
    } else if (filter.filterType == "MIN_NOTIONAL") {
      if (isTakerOrder) {
        if (filter.applyToMarket) {
          priceForNotional = computePriceForNotional(mk, filter.avgPriceMins);
          pMinNotionalFilter = std::addressof(filter);
        }
      } else {
        pMinNotionalFilter = std::addressof(filter);
      }
    } else if (filter.filterType == "NOTIONAL") {
      if (isTakerOrder) {
        if (filter.applyMinToMarket || filter.applyMaxToMarket) {
          priceForNotional = computePriceForNotional(mk, filter.avgPriceMins);
          pNotionalFilter = std::addressof(filter);
        }
      } else {
        pNotionalFilter = std::addressof(filter);
      }
    }
  }

  MonetaryAmount minVolumeAfterMinNotional(0, ret.currencyCode());
  if (pMinNotionalFilter != nullptr) {
    MonetaryAmount minNotional(pMinNotionalFilter->minNotional);
    MonetaryAmount priceTimesQuantity = ret.toNeutral() * priceForNotional.toNeutral();

    minVolumeAfterMinNotional = MonetaryAmount(minNotional / priceForNotional, ret.currencyCode());
    if (priceTimesQuantity < minNotional) {
      log::debug("Too small min price * quantity. {} increased to {} for {}", ret, minVolumeAfterMinNotional, mk);
      ret = minVolumeAfterMinNotional;
    }
  }

  if (pNotionalFilter != nullptr) {
    MonetaryAmount priceTimesQuantity = ret.toNeutral() * priceForNotional.toNeutral();

    if (!isTakerOrder || pNotionalFilter->applyMinToMarket) {
      // min notional applies
      MonetaryAmount minNotional(pNotionalFilter->minNotional);

      minVolumeAfterMinNotional =
          std::max(minVolumeAfterMinNotional, MonetaryAmount(minNotional / priceForNotional, ret.currencyCode()));

      if (priceTimesQuantity < minNotional) {
        log::debug("Too small (price * quantity). {} increased to {} for {}", ret, minVolumeAfterMinNotional, mk);
        ret = minVolumeAfterMinNotional;
      }
    } else if (!isTakerOrder || pNotionalFilter->applyMaxToMarket) {
      // max notional applies
      MonetaryAmount maxNotional(pNotionalFilter->maxNotional);
      MonetaryAmount maxVolumeAfterMaxNotional = MonetaryAmount(maxNotional / priceForNotional, ret.currencyCode());

      if (priceTimesQuantity > maxNotional) {
        log::debug("Too large (price * quantity). {} decreased to {} for {}", ret, maxVolumeAfterMaxNotional, mk);
        ret = maxVolumeAfterMaxNotional;
      }
    }
  }

  for (const auto* pLotFilterPtr : {pMarketLotSizeFilter, pLotSizeFilter}) {
    if (pLotFilterPtr != nullptr) {
      // "maxQty": "9000000.00000000",
      // "minQty": "1.00000000",
      // "stepSize": "1.00000000"
      MonetaryAmount maxQty(pLotFilterPtr->maxQty, ret.currencyCode());
      MonetaryAmount minQty(pLotFilterPtr->minQty, ret.currencyCode());
      MonetaryAmount stepSize(pLotFilterPtr->stepSize, ret.currencyCode());

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
  auto result = PublicQuery<schema::binance::V3TickerBookTicker>(_commonInfo._curlHandle, "/api/v3/ticker/bookTicker");
  using BinanceAssetPairToStdMarketMap = std::unordered_map<string, Market>;
  BinanceAssetPairToStdMarketMap binanceAssetPairToStdMarketMap;
  binanceAssetPairToStdMarketMap.reserve(markets.size());
  for (Market mk : markets) {
    binanceAssetPairToStdMarketMap.insert_or_assign(mk.assetsPairStrUpper(), mk);
  }
  const auto time = Clock::now();
  for (const auto& elem : result) {
    auto it = binanceAssetPairToStdMarketMap.find(elem.symbol);
    if (it == binanceAssetPairToStdMarketMap.end()) {
      continue;
    }
    Market mk = it->second;
    MonetaryAmount askPri(elem.askPrice, mk.quote());
    MonetaryAmount bidPri(elem.bidPrice, mk.quote());
    MonetaryAmount askVol(elem.askQty, mk.base());
    MonetaryAmount bidVol(elem.bidQty, mk.base());

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
  const auto asksAndBids =
      PublicQuery<schema::binance::V3OrderBook>(_commonInfo._curlHandle, "/api/v3/depth", postData);
  const auto nowTime = Clock::now();

  orderBookLines.reserve(std::min(static_cast<decltype(depth)>(asksAndBids.asks.size()), depth) +
                         std::min(static_cast<decltype(depth)>(asksAndBids.bids.size()), depth));

  for (const auto asksOrBids : {&asksAndBids.asks, &asksAndBids.bids}) {
    const auto type = asksOrBids == &asksAndBids.asks ? OrderBookLine::Type::kAsk : OrderBookLine::Type::kBid;
    for (const auto& [pri, vol] : *asksOrBids | std::ranges::views::take(depth)) {
      MonetaryAmount price(pri, mk.quote());
      MonetaryAmount amount(vol, mk.base());

      orderBookLines.push(amount, price, type);
    }
  }

  return MarketOrderBook(nowTime, mk, orderBookLines);
}

MonetaryAmount BinancePublic::TradedVolumeFunc::operator()(Market mk) {
  const auto result = PublicQuery<schema::binance::V3Ticker24hr>(_commonInfo._curlHandle, "/api/v3/ticker/24hr",
                                                                 {{"symbol", mk.assetsPairStrUpper()}});

  return {result.volume, mk.base()};
}

PublicTradeVector BinancePublic::queryLastTrades(Market mk, int nbTrades) {
  static constexpr int kMaxNbLastTrades = 1000;

  if (nbTrades > kMaxNbLastTrades) {
    log::warn("{} is larger than maximum number of last trades of {} on {}", nbTrades, kMaxNbLastTrades, name());
    nbTrades = kMaxNbLastTrades;
  }

  const auto result = PublicQuery<schema::binance::V3Trades>(
      _commonInfo._curlHandle, "/api/v3/trades", {{"symbol", mk.assetsPairStrUpper()}, {"limit", nbTrades}});

  PublicTradeVector ret;
  ret.reserve(static_cast<PublicTradeVector::size_type>(result.size()));

  for (const auto& elem : result) {
    MonetaryAmount price(elem.price, mk.quote());
    MonetaryAmount amount(elem.qty, mk.base());
    int64_t millisecondsSinceEpoch = elem.time;
    TradeSide tradeSide = elem.isBuyerMaker ? TradeSide::kSell : TradeSide::kBuy;

    ret.emplace_back(tradeSide, amount, price, TimePoint(milliseconds(millisecondsSinceEpoch)));
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount BinancePublic::TickerFunc::operator()(Market mk) {
  const auto data = PublicQuery<schema::binance::V3TickerPrice>(_commonInfo._curlHandle, "/api/v3/ticker/price",
                                                                {{"symbol", mk.assetsPairStrUpper()}});
  return {data.price, mk.quote()};
}

}  // namespace cct::api
