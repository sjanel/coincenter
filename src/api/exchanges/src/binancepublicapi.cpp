#include "binancepublicapi.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <execution>
#include <thread>
#include <unordered_map>

#include "apikey.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "codec.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "monetaryamount.hpp"
#include "ssl_sha.hpp"

namespace cct::api {
namespace {

constexpr int kMaxNbLastTrades = 1000;

json PublicQuery(CurlHandle& curlHandle, std::string_view method, const CurlPostData& curlPostData = CurlPostData()) {
  string endpoint(method);
  if (!curlPostData.empty()) {
    endpoint.push_back('?');
    endpoint.append(curlPostData.str());
  }
  json ret = json::parse(curlHandle.query(endpoint, CurlOptions(HttpRequestType::kGet, BinancePublic::kUserAgent)));
  auto foundErrorIt = ret.find("code");
  auto foundMsgIt = ret.find("msg");
  if (foundErrorIt != ret.end() && foundMsgIt != ret.end()) {
    const int statusCode = foundErrorIt->get<int>();  // "1100" for instance
    log::error("Full Binance json error: '{}'", ret.dump());
    string ex("Error: ");
    ex.append(MonetaryAmount(statusCode).amountStr());
    ex.append(", msg: ");
    ex.append(foundMsgIt->get<std::string_view>());
    throw exception(std::move(ex));
  }
  return ret;
}

template <class ExchangeInfoDataByMarket>
const json& RetrieveMarketData(const ExchangeInfoDataByMarket& exchangeInfoData, Market m) {
  auto it = exchangeInfoData.find(m);
  if (it == exchangeInfoData.end()) {
    string ex("Unable to retrieve market data ");
    ex.append(m.str());
    throw exception(std::move(ex));
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

BinancePublic::BinancePublic(const CoincenterInfo& coincenterInfo, FiatConverter& fiatConverter,
                             api::CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("binance", fiatConverter, cryptowatchAPI, coincenterInfo),
      _commonInfo(coincenterInfo, exchangeInfo(), coincenterInfo.getRunMode()),
      _exchangeInfoCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault),
                         _commonInfo),
      _globalInfosCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault),
          coincenterInfo.metricGatewayPtr(), _commonInfo._exchangeInfo.publicAPIRate(), coincenterInfo.getRunMode()),
      _marketsCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kMarkets), _cachedResultVault),
                    _exchangeInfoCache, _commonInfo._curlHandle, _commonInfo._exchangeInfo),
      _allOrderBooksCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kAllOrderBooks), _cachedResultVault),
          _exchangeInfoCache, _marketsCache, _commonInfo),
      _orderbookCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kOrderBook), _cachedResultVault),
                      _commonInfo),
      _tradedVolumeCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kTradedVolume), _cachedResultVault),
          _commonInfo),
      _tickerCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kLastPrice), _cachedResultVault),
                   _commonInfo) {}

BinancePublic::CommonInfo::CommonInfo(const CoincenterInfo& coincenterInfo, const ExchangeInfo& exchangeInfo,
                                      settings::RunMode runMode)
    : _exchangeInfo(exchangeInfo),
      _curlHandle(kURLBases, coincenterInfo.metricGatewayPtr(), _exchangeInfo.publicAPIRate(), runMode) {}

CurrencyExchangeFlatSet BinancePublic::queryTradableCurrencies(const json& data) const {
  CurrencyExchangeVector currencies;
  const ExchangeInfo::CurrencySet& excludedCurrencies = _commonInfo._exchangeInfo.excludedCurrenciesAll();
  for (const json& el : data) {
    std::string_view coin = el["coin"].get<std::string_view>();
    if (coin.size() > CurrencyCode::kAcronymMaxLen) {
      continue;
    }
    CurrencyCode cur(coin);
    if (excludedCurrencies.contains(cur)) {
      log::trace("Discard {} excluded by config", cur.str());
      continue;
    }
    bool isFiat = el["isLegalMoney"];
    const auto& networkList = el["networkList"];
    if (networkList.size() > 1) {
      log::debug("Several networks found for {}, considering only default network", cur.str());
    }
    for (const json& networkDetail : networkList) {
      bool isDefault = networkDetail["isDefault"].get<bool>();
      if (isDefault) {
        bool withdrawEnable = networkDetail["withdrawEnable"].get<bool>();
        bool depositEnable = networkDetail["depositEnable"].get<bool>();
        currencies.emplace_back(
            cur, cur, cur,
            depositEnable ? CurrencyExchange::Deposit::kAvailable : CurrencyExchange::Deposit::kUnavailable,
            withdrawEnable ? CurrencyExchange::Withdraw::kAvailable : CurrencyExchange::Withdraw::kUnavailable,
            isFiat ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto);
        break;
      }
    }
  }
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} {} currencies", _name, ret.size());
  return ret;
}

MarketSet BinancePublic::MarketsFunc::operator()() {
  BinancePublic::ExchangeInfoFunc::ExchangeInfoDataByMarket exchangeInfoData = _exchangeInfoCache.get();
  const ExchangeInfo::CurrencySet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  MarketSet ret;
  ret.reserve(static_cast<MarketSet::size_type>(exchangeInfoData.size()));
  for (const auto& marketJsonPair : exchangeInfoData) {
    const json& symbol = marketJsonPair.second;
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
  json exchangeInfoData = PublicQuery(_commonInfo._curlHandle, "/api/v3/exchangeInfo");
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
  string s = _curlHandle.query("", CurlOptions(HttpRequestType::kGet));
  // This json is HUGE and contains numerous amounts of information
  static constexpr std::string_view appBegJson = "application/json\">";
  string::const_iterator first = s.begin() + s.find(appBegJson) + appBegJson.size();
  std::string_view sv(first, s.end());
  std::size_t reduxPos = sv.find("redux\":");
  std::size_t ssrStorePos = sv.find("ssrStore\":", reduxPos);
  static constexpr std::string_view kCryptoFeeStart = "cryptoFee\":";
  std::size_t cryptoFeePos = sv.find(kCryptoFeeStart, ssrStorePos);

  std::size_t startPos = cryptoFeePos + kCryptoFeeStart.size();

  sv = std::string_view(sv.begin() + startPos, sv.end());

  const std::size_t svSize = sv.size();

  std::size_t endPos = 1;
  int squareBracketCount = 1;
  for (; endPos < svSize && squareBracketCount != 0; ++endPos) {
    switch (sv[endPos]) {
      case '[':
        ++squareBracketCount;
        break;
      case ']':
        --squareBracketCount;
        break;
      default:
        break;
    }
  }
  if (squareBracketCount != 0) {
    throw exception("JSON parsing error from Binance cryptoFee scraper");
  }

  return json::parse(std::string_view(sv.data(), endPos));
}

namespace {
MonetaryAmount ComputeWithdrawalFeesFromNetworkList(CurrencyCode cur, const json& networkList) {
  MonetaryAmount withdrawFee(0, cur);
  for (const json& networkListPart : networkList) {
    withdrawFee = std::max(withdrawFee, MonetaryAmount(networkListPart["withdrawFee"].get<std::string_view>(), cur));
  }
  return withdrawFee;
}
}  // namespace

WithdrawalFeeMap BinancePublic::queryWithdrawalFees() {
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
  string ex("Unable to find withdrawal fee for ");
  ex.append(currencyCode.str());
  throw exception(std::move(ex));
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
      ret.round(tickSize, MonetaryAmount::RoundType::kDown);
      if (ret != pri) {
        log::debug("Rounded {} into {} according to {}", pri.str(), ret.str(), m.str());
      }
    }
  }

  VolAndPriNbDecimals volAndPriNbDecimals = QueryVolAndPriNbDecimals(_exchangeInfoCache.get(), m);
  ret.truncate(volAndPriNbDecimals.priNbDecimals);
  if (pri != ret) {
    log::warn("Sanitize price {} -> {}", pri.str(), ret.str());
  }
  return ret;
}

MonetaryAmount BinancePublic::sanitizeVolume(Market m, MonetaryAmount vol, MonetaryAmount priceForMinNotional,
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
      if (isTakerOrder) {
        const bool appliesToMarketTrade = filter["applyToMarket"].get<bool>();
        if (appliesToMarketTrade) {
          const int avgPriceMins = filter["avgPriceMins"].get<int>();

          if (avgPriceMins == 0) {
            // price should be the last matched price
            LastTradesVector lastTrades = queryLastTrades(m, 1);
            if (lastTrades.empty()) {
              log::error("Unable to retrieve last trades from {}, use average price instead for min notional", m.str());
              json result =
                  PublicQuery(_commonInfo._curlHandle, "/api/v3/avgPrice", {{"symbol", m.assetsPairStrUpper()}});
              priceForMinNotional = MonetaryAmount(result["price"].get<std::string_view>(), m.quote());
            } else {
              priceForMinNotional = lastTrades.front().price();
            }
          } else {
            json result =
                PublicQuery(_commonInfo._curlHandle, "/api/v3/avgPrice", {{"symbol", m.assetsPairStrUpper()}});
            priceForMinNotional = MonetaryAmount(result["price"].get<std::string_view>(), m.quote());
          }
          minNotionalFilter = std::addressof(filter);
        }
      } else {
        minNotionalFilter = std::addressof(filter);
      }
    }
  }

  MonetaryAmount minVolumeAfterMinNotional(0, ret.currencyCode());
  if (minNotionalFilter) {
    // "applyToMarket": true,
    // "avgPriceMins": 5,
    // "minNotional": "0.05000000"
    MonetaryAmount minNotional((*minNotionalFilter)["minNotional"].get<std::string_view>());
    MonetaryAmount priceTimesQuantity = ret.toNeutral() * priceForMinNotional.toNeutral();
    minVolumeAfterMinNotional = MonetaryAmount(minNotional / priceForMinNotional, ret.currencyCode());
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
        ret.round(stepSize, MonetaryAmount::RoundType::kDown);
        if (ret < minVolumeAfterMinNotional) {
          ret.round(stepSize, MonetaryAmount::RoundType::kUp);
        }
        if (ret != vol) {
          log::debug("Rounded {} into {} according to {}", vol.str(), ret.str(), m.str());
        }
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

MarketOrderBookMap BinancePublic::AllOrderBooksFunc::operator()(int depth) {
  MarketOrderBookMap ret;
  const MarketSet& markets = _marketsCache.get();
  json result = PublicQuery(_commonInfo._curlHandle, "/api/v3/ticker/bookTicker");
  using BinanceAssetPairToStdMarketMap = std::unordered_map<string, Market>;
  BinanceAssetPairToStdMarketMap binanceAssetPairToStdMarketMap;
  binanceAssetPairToStdMarketMap.reserve(markets.size());
  for (Market m : markets) {
    binanceAssetPairToStdMarketMap.insert_or_assign(m.assetsPairStrUpper(), m);
  }
  for (const json& tickerDetails : result) {
    string assetsPairStr = tickerDetails["symbol"];
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
  static constexpr int kAuthorizedDepths[] = {5, 10, 20, 50, 100, 500, 1000, 5000};
  auto lb = std::ranges::lower_bound(kAuthorizedDepths, depth);
  if (lb == std::end(kAuthorizedDepths)) {
    lb = std::next(std::end(kAuthorizedDepths), -1);
    log::error("Invalid depth {}, default to {}", depth, *lb);
  }
  CurlPostData postData{{"symbol", m.assetsPairStrUpper()}, {"limit", *lb}};
  json asksAndBids = PublicQuery(_commonInfo._curlHandle, "/api/v3/depth", postData);
  const json& asks = asksAndBids["asks"];
  const json& bids = asksAndBids["bids"];
  using OrderBookVec = vector<OrderBookLine>;
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
  json result = PublicQuery(_commonInfo._curlHandle, "/api/v3/ticker/24hr", {{"symbol", m.assetsPairStrUpper()}});
  std::string_view last24hVol = result["volume"].get<std::string_view>();
  return MonetaryAmount(last24hVol, m.base());
}

LastTradesVector BinancePublic::queryLastTrades(Market m, int nbTrades) {
  if (nbTrades > kMaxNbLastTrades) {
    log::warn("{} is larger than maximum number of last trades of {} on {}", nbTrades, kMaxNbLastTrades, _name);
    nbTrades = kMaxNbLastTrades;
  }
  json result =
      PublicQuery(_commonInfo._curlHandle, "/api/v3/trades", {{"symbol", m.assetsPairStrUpper()}, {"limit", nbTrades}});

  LastTradesVector ret;
  ret.reserve(static_cast<LastTradesVector::size_type>(result.size()));
  for (const json& detail : result) {
    MonetaryAmount amount(detail["qty"].get<std::string_view>(), m.base());
    MonetaryAmount price(detail["price"].get<std::string_view>(), m.quote());
    int64_t millisecondsSinceEpoch = detail["time"].get<int64_t>();
    TradeSide tradeSide = detail["isBuyerMaker"].get<bool>() ? TradeSide::kSell : TradeSide::kBuy;

    ret.emplace_back(tradeSide, amount, price, TimePoint(std::chrono::milliseconds(millisecondsSinceEpoch)));
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount BinancePublic::TickerFunc::operator()(Market m) {
  json result = PublicQuery(_commonInfo._curlHandle, "/api/v3/ticker/price", {{"symbol", m.assetsPairStrUpper()}});
  std::string_view lastPrice = result["price"].get<std::string_view>();
  return MonetaryAmount(lastPrice, m.quote());
}

}  // namespace cct::api
