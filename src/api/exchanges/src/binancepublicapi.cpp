#include "binancepublicapi.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "cachedresult.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
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
#include "invariant-request-retry.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "permanentcurloptions.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct::api {
namespace {

constexpr int kMaxNbLastTrades = 1000;

json PublicQuery(CurlHandle& curlHandle, std::string_view method, const CurlPostData& curlPostData = CurlPostData()) {
  string endpoint(method);
  if (!curlPostData.empty()) {
    endpoint.push_back('?');
    endpoint.append(curlPostData.str());
  }
  InvariantRequestRetry requestRetry(curlHandle, endpoint, CurlOptions(HttpRequestType::kGet));

  return requestRetry.queryJson([](const json& jsonResponse) {
    const auto foundErrorIt = jsonResponse.find("code");
    const auto foundMsgIt = jsonResponse.find("msg");
    if (foundErrorIt != jsonResponse.end() && foundMsgIt != jsonResponse.end()) {
      const int statusCode = foundErrorIt->get<int>();  // "1100" for instance
      log::warn("Binance error ({}), full json: '{}'", statusCode, jsonResponse.dump());
      return InvariantRequestRetry::Status::kResponseError;
    }
    return InvariantRequestRetry::Status::kResponseOK;
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
      _globalInfosCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault),
          coincenterInfo.metricGatewayPtr(),
          PermanentCurlOptions::Builder()
              .setMinDurationBetweenQueries(_commonInfo._exchangeConfig.publicAPIRate())
              .setAcceptedEncoding(_commonInfo._exchangeConfig.acceptEncoding())
              .build(),
          coincenterInfo.getRunMode()),
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
  json result = json::parse(_commonInfo._curlHandle.query("/api/v3/ping", CurlOptions(HttpRequestType::kGet)));
  if (!result.empty()) {
    log::error("{} health check is not empty: {}", _name, result.dump());
  }
  return result.empty();
}

BinancePublic::CommonInfo::CommonInfo(const CoincenterInfo& coincenterInfo, const ExchangeConfig& exchangeConfig,
                                      settings::RunMode runMode)
    : _exchangeConfig(exchangeConfig),
      _curlHandle(kURLBases, coincenterInfo.metricGatewayPtr(),
                  PermanentCurlOptions::Builder()
                      .setMinDurationBetweenQueries(_exchangeConfig.publicAPIRate())
                      .setAcceptedEncoding(_exchangeConfig.acceptEncoding())
                      .setRequestCallLogLevel(_exchangeConfig.requestsCallLogLevel())
                      .setRequestAnswerLogLevel(_exchangeConfig.requestsAnswerLogLevel())
                      .build(),
                  runMode) {}

CurrencyExchangeFlatSet BinancePublic::queryTradableCurrencies(const json& data) const {
  CurrencyExchangeVector currencies;
  const CurrencyCodeSet& excludedCurrencies = _commonInfo._exchangeConfig.excludedCurrenciesAll();
  for (const json& coinJson : data) {
    std::string_view coin = coinJson["coin"].get<std::string_view>();
    if (coin.size() > CurrencyCode::kMaxLen) {
      continue;
    }
    CurrencyCode cur(coin);
    if (excludedCurrencies.contains(cur)) {
      log::trace("Discard {} excluded by config", cur.str());
      continue;
    }
    bool isFiat = coinJson["isLegalMoney"];
    const auto& networkList = coinJson["networkList"];
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
  BinancePublic::ExchangeInfoFunc::ExchangeInfoDataByMarket exchangeInfoData = _exchangeConfigCache.get();
  const CurrencyCodeSet& excludedCurrencies = _exchangeConfig.excludedCurrenciesAll();
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
    log::debug("Accept {}-{} Binance asset pair", baseAsset, quoteAsset);
    ret.insert_or_assign(Market(baseAsset, quoteAsset), std::move(*it));
  }
  return ret;
}

namespace {
constexpr std::string_view kCryptoFeeBaseUrl = "https://www.binance.com";
}

BinancePublic::GlobalInfosFunc::GlobalInfosFunc(AbstractMetricGateway* pMetricGateway,
                                                const PermanentCurlOptions& permanentCurlOptions,
                                                settings::RunMode runMode)
    : _curlHandle(kCryptoFeeBaseUrl, pMetricGateway, permanentCurlOptions, runMode) {}

json BinancePublic::GlobalInfosFunc::operator()() {
  return PublicQuery(_curlHandle, "/bapi/capital/v1/public/capital/getNetworkCoinAll").at("data");
}

namespace {
MonetaryAmount ComputeWithdrawalFeesFromNetworkList(CurrencyCode cur, const json& networkList) {
  MonetaryAmount withdrawFee(0, cur);
  for (const json& networkListPart : networkList) {
    MonetaryAmount fee(networkListPart["withdrawFee"].get<std::string_view>(), cur);
    auto isDefaultIt = networkListPart.find("isDefault");
    if (isDefaultIt != networkListPart.end() && isDefaultIt->get<bool>()) {
      withdrawFee = fee;
      break;
    }
    withdrawFee = std::max(withdrawFee, fee);
  }
  return withdrawFee;
}
}  // namespace

MonetaryAmountByCurrencySet BinancePublic::queryWithdrawalFees() {
  vector<MonetaryAmount> fees;
  for (const json& coinJson : _globalInfosCache.get()) {
    std::string_view coinStr = coinJson["coin"].get<std::string_view>();
    if (coinStr.size() > CurrencyCode::kMaxLen) {
      continue;
    }
    CurrencyCode cur(coinStr);
    MonetaryAmount withdrawFee = ComputeWithdrawalFeesFromNetworkList(cur, coinJson["networkList"]);
    log::trace("Retrieved {} withdrawal fee {}", _name, withdrawFee);
    fees.push_back(std::move(withdrawFee));
  }

  log::info("Retrieved {} withdrawal fees for {} coins", _name, fees.size());
  assert(!fees.empty());
  return MonetaryAmountByCurrencySet(std::move(fees));
}

std::optional<MonetaryAmount> BinancePublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  for (const json& el : _globalInfosCache.get()) {
    CurrencyCode cur(el["coin"].get<std::string_view>());
    if (cur == currencyCode) {
      return ComputeWithdrawalFeesFromNetworkList(cur, el["networkList"]);
    }
  }
  return {};
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
    LastTradesVector lastTrades = queryLastTrades(mk, 1);
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

    ret.insert_or_assign(mk, MarketOrderBook(askPri, askVol, bidPri, bidVol,
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
  using OrderBookVec = vector<OrderBookLine>;
  OrderBookVec orderBookLines;

  CurlPostData postData{{"symbol", mk.assetsPairStrUpper()}, {"limit", *lb}};
  json asksAndBids = PublicQuery(_commonInfo._curlHandle, "/api/v3/depth", postData);
  const auto asksIt = asksAndBids.find("asks");
  const auto bidsIt = asksAndBids.find("bids");

  if (asksIt != asksAndBids.end() && bidsIt != asksAndBids.end()) {
    orderBookLines.reserve(static_cast<OrderBookVec::size_type>(asksIt->size() + bidsIt->size()));
    for (const auto& asksOrBids : {asksIt, bidsIt}) {
      const bool isAsk = asksOrBids == asksIt;
      for (const auto& priceQuantityPair : *asksOrBids) {
        MonetaryAmount amount(priceQuantityPair.back().get<std::string_view>(), mk.base());
        MonetaryAmount price(priceQuantityPair.front().get<std::string_view>(), mk.quote());

        orderBookLines.emplace_back(amount, price, isAsk);
      }
    }
  }

  return MarketOrderBook(mk, orderBookLines);
}

MonetaryAmount BinancePublic::TradedVolumeFunc::operator()(Market mk) {
  const json result =
      PublicQuery(_commonInfo._curlHandle, "/api/v3/ticker/24hr", {{"symbol", mk.assetsPairStrUpper()}});
  const auto volumeIt = result.find("volume");
  const std::string_view last24hVol = volumeIt == result.end() ? std::string_view() : volumeIt->get<std::string_view>();

  return {last24hVol, mk.base()};
}

LastTradesVector BinancePublic::queryLastTrades(Market mk, int nbTrades) {
  if (nbTrades > kMaxNbLastTrades) {
    log::warn("{} is larger than maximum number of last trades of {} on {}", nbTrades, kMaxNbLastTrades, _name);
    nbTrades = kMaxNbLastTrades;
  }
  json result = PublicQuery(_commonInfo._curlHandle, "/api/v3/trades",
                            {{"symbol", mk.assetsPairStrUpper()}, {"limit", nbTrades}});

  LastTradesVector ret;
  ret.reserve(static_cast<LastTradesVector::size_type>(result.size()));
  for (const json& detail : result) {
    MonetaryAmount amount(detail["qty"].get<std::string_view>(), mk.base());
    MonetaryAmount price(detail["price"].get<std::string_view>(), mk.quote());
    int64_t millisecondsSinceEpoch = detail["time"].get<int64_t>();
    TradeSide tradeSide = detail["isBuyerMaker"].get<bool>() ? TradeSide::kSell : TradeSide::kBuy;

    ret.emplace_back(tradeSide, amount, price, TimePoint(std::chrono::milliseconds(millisecondsSinceEpoch)));
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
