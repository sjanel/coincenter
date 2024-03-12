
#include "huobipublicapi.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "cachedresult.hpp"
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
#include "monetaryamountbycurrencyset.hpp"
#include "permanentcurloptions.hpp"
#include "timedef.hpp"
#include "toupperlower-string.hpp"
#include "tradeside.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct::api {
namespace {

constexpr std::string_view kHealthCheckBaseUrl[] = {"https://status.huobigroup.com"};

json PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, const CurlPostData& curlPostData = CurlPostData()) {
  string method(endpoint);
  if (!curlPostData.empty()) {
    method.push_back('?');
    method.append(curlPostData.str());
  }

  InvariantRequestRetry requestRetry(curlHandle, method, CurlOptions(HttpRequestType::kGet));

  json jsonResponse = requestRetry.queryJson([](const json& jsonResponse) {
    const auto dataIt = jsonResponse.find("data");
    if (dataIt == jsonResponse.end()) {
      const auto tickIt = jsonResponse.find("tick");
      if (tickIt == jsonResponse.end()) {
        log::warn("Full Huobi json error: '{}'", jsonResponse.dump());
        return InvariantRequestRetry::Status::kResponseError;
      }
    }
    return InvariantRequestRetry::Status::kResponseOK;
  });
  json ret;
  const auto dataIt = jsonResponse.find("data");
  if (dataIt != jsonResponse.end()) {
    ret.swap(*dataIt);
  } else {
    const auto tickIt = jsonResponse.find("tick");
    if (tickIt != jsonResponse.end()) {
      ret.swap(*tickIt);
    }
  }
  return ret;
}

}  // namespace

HuobiPublic::HuobiPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CommonAPI& commonAPI)
    : ExchangePublic("huobi", fiatConverter, commonAPI, config),
      _exchangeConfig(config.exchangeConfig(_name)),
      _curlHandle(kURLBases, config.metricGatewayPtr(),
                  PermanentCurlOptions::Builder()
                      .setMinDurationBetweenQueries(_exchangeConfig.publicAPIRate())
                      .setAcceptedEncoding(_exchangeConfig.acceptEncoding())
                      .setRequestCallLogLevel(_exchangeConfig.requestsCallLogLevel())
                      .setRequestAnswerLogLevel(_exchangeConfig.requestsAnswerLogLevel())
                      .build(),
                  config.getRunMode()),
      _healthCheckCurlHandle(
          kHealthCheckBaseUrl, config.metricGatewayPtr(),
          PermanentCurlOptions::Builder().setMinDurationBetweenQueries(_exchangeConfig.publicAPIRate()).build(),
          config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(_exchangeConfig.getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), _curlHandle),
      _marketsCache(CachedResultOptions(_exchangeConfig.getAPICallUpdateFrequency(kMarkets), _cachedResultVault),
                    _curlHandle, _exchangeConfig),
      _allOrderBooksCache(
          CachedResultOptions(_exchangeConfig.getAPICallUpdateFrequency(kAllOrderBooks), _cachedResultVault),
          _marketsCache, _curlHandle, _exchangeConfig),
      _orderbookCache(CachedResultOptions(_exchangeConfig.getAPICallUpdateFrequency(kOrderBook), _cachedResultVault),
                      _curlHandle, _exchangeConfig),
      _tradedVolumeCache(
          CachedResultOptions(_exchangeConfig.getAPICallUpdateFrequency(kTradedVolume), _cachedResultVault),
          _curlHandle),
      _tickerCache(CachedResultOptions(_exchangeConfig.getAPICallUpdateFrequency(kLastPrice), _cachedResultVault),
                   _curlHandle) {}

bool HuobiPublic::healthCheck() {
  json result = json::parse(_healthCheckCurlHandle.query("/api/v2/summary.json", CurlOptions(HttpRequestType::kGet)));
  auto statusIt = result.find("status");
  if (statusIt == result.end()) {
    log::error("Unexpected answer from {} status: {}", _name, result.dump());
    return false;
  }
  auto descriptionIt = statusIt->find("description");
  if (descriptionIt == statusIt->end()) {
    log::error("Unexpected answer from {} status: {}", _name, statusIt->dump());
    return false;
  }
  std::string_view statusStr = descriptionIt->get<std::string_view>();
  log::info("{} status: {}", _name, statusStr);
  auto incidentsIt = result.find("incidents");
  return incidentsIt != result.end() && incidentsIt->empty();
}

json HuobiPublic::TradableCurrenciesFunc::operator()() { return PublicQuery(_curlHandle, "/v2/reference/currencies"); }

bool HuobiPublic::ShouldDiscardChain(CurrencyCode cur, const json& chainDetail) {
  std::string_view chainName = chainDetail["chain"].get<std::string_view>();
  if (!cur.iequal(chainName) && !cur.iequal(chainDetail["displayName"].get<std::string_view>())) {
    log::debug("Discarding chain '{}' as not supported by {}", chainName, cur);
    return true;
  }
  return false;
}

HuobiPublic::WithdrawParams HuobiPublic::getWithdrawParams(CurrencyCode cur) {
  WithdrawParams withdrawParams;
  for (const json& curDetail : _tradableCurrenciesCache.get()) {
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    if (cur == CurrencyCode(_coincenterInfo.standardizeCurrencyCode(curStr))) {
      for (const json& chainDetail : curDetail["chains"]) {
        if (ShouldDiscardChain(cur, chainDetail)) {
          continue;
        }

        withdrawParams.minWithdrawAmt = MonetaryAmount(chainDetail["minWithdrawAmt"].get<std::string_view>(), cur);
        withdrawParams.maxWithdrawAmt = MonetaryAmount(chainDetail["maxWithdrawAmt"].get<std::string_view>(), cur);
        withdrawParams.withdrawPrecision = chainDetail["withdrawPrecision"].get<int8_t>();

        return withdrawParams;
      }
      break;
    }
  }
  log::error("Unable to find {} chain for withdraw parameters on Huobi. Use default values");
  return withdrawParams;
}

CurrencyExchangeFlatSet HuobiPublic::queryTradableCurrencies() {
  CurrencyExchangeVector currencies;
  for (const json& curDetail : _tradableCurrenciesCache.get()) {
    std::string_view statusStr = curDetail["instStatus"].get<std::string_view>();
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    if (statusStr != "normal") {
      log::debug("{} is {} from Huobi", curStr, statusStr);
      continue;
    }
    bool foundChainWithSameName = false;
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    for (const json& chainDetail : curDetail["chains"]) {
      if (ShouldDiscardChain(cur, chainDetail)) {
        continue;
      }
      std::string_view depositAllowedStr = chainDetail["depositStatus"].get<std::string_view>();
      std::string_view withdrawAllowedStr = chainDetail["withdrawStatus"].get<std::string_view>();
      auto depositAllowed = depositAllowedStr == "allowed" ? CurrencyExchange::Deposit::kAvailable
                                                           : CurrencyExchange::Deposit::kUnavailable;
      auto withdrawAllowed = withdrawAllowedStr == "allowed" ? CurrencyExchange::Withdraw::kAvailable
                                                             : CurrencyExchange::Withdraw::kUnavailable;
      auto curExchangeType =
          _commonApi.queryIsCurrencyCodeFiat(cur) ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto;

      CurrencyExchange newCurrency(cur, curStr, curStr, depositAllowed, withdrawAllowed, curExchangeType);

      log::debug("Retrieved Huobi Currency {}", newCurrency.str());
      currencies.push_back(std::move(newCurrency));

      foundChainWithSameName = true;
      break;
    }
    if (!foundChainWithSameName) {
      log::debug("Cannot find {} main chain in Huobi, discarding currency", cur);
    }
  }
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} Huobi currencies", ret.size());
  return ret;
}

std::pair<MarketSet, HuobiPublic::MarketsFunc::MarketInfoMap> HuobiPublic::MarketsFunc::operator()() {
  json result = PublicQuery(_curlHandle, "/v1/common/symbols");

  MarketSet markets;
  MarketInfoMap marketInfoMap;

  markets.reserve(static_cast<MarketSet::size_type>(result.size()));

  const CurrencyCodeSet& excludedCurrencies = _exchangeConfig.excludedCurrenciesAll();

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
    if (baseAsset.size() > CurrencyCode::kMaxLen || quoteAsset.size() > CurrencyCode::kMaxLen) {
      log::trace("Discard {}-{} as one asset is too long", baseAsset, quoteAsset);
      continue;
    }
    log::trace("Accept {}-{} Huobi asset pair", baseAsset, quoteAsset);
    CurrencyCode base(baseAsset);
    CurrencyCode quote(quoteAsset);
    Market mk(base, quote);
    markets.insert(mk);

    int8_t volNbDec = marketDetails["amount-precision"].get<int8_t>();
    int8_t priNbDec = marketDetails["price-precision"].get<int8_t>();
    // value = vol * pri in quote currency
    MarketInfo marketInfo;

    marketInfo.volAndPriNbDecimals = {volNbDec, priNbDec};

    marketInfo.minOrderValue = MonetaryAmount(marketDetails["min-order-value"].get<double>(), quote);
    auto maxOrderValueIt = marketDetails.find("max-order-value");
    if (maxOrderValueIt != marketDetails.end()) {  // in USDT
      marketInfo.maxOrderValueUSDT = MonetaryAmount(maxOrderValueIt->get<double>(), "USDT");
    }

    marketInfo.limitMinOrderAmount = MonetaryAmount(marketDetails["limit-order-min-order-amt"].get<double>(), base);
    marketInfo.limitMaxOrderAmount = MonetaryAmount(marketDetails["limit-order-max-order-amt"].get<double>(), base);

    marketInfo.sellMarketMinOrderAmount =
        MonetaryAmount(marketDetails["sell-market-min-order-amt"].get<double>(), base);
    marketInfo.sellMarketMaxOrderAmount =
        MonetaryAmount(marketDetails["sell-market-max-order-amt"].get<double>(), base);

    marketInfo.buyMarketMaxOrderValue =
        MonetaryAmount(marketDetails["buy-market-max-order-value"].get<double>(), quote);

    marketInfoMap.insert_or_assign(mk, std::move(marketInfo));
  }
  log::info("Retrieved huobi {} markets", markets.size());
  return {std::move(markets), std::move(marketInfoMap)};
}

MonetaryAmountByCurrencySet HuobiPublic::queryWithdrawalFees() {
  vector<MonetaryAmount> fees;
  for (const json& curDetail : _tradableCurrenciesCache.get()) {
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    bool foundChainWithSameName = false;
    for (const json& chainDetail : curDetail["chains"]) {
      if (ShouldDiscardChain(cur, chainDetail)) {
        continue;
      }
      auto withdrawFeeTypeIt = chainDetail.find("withdrawFeeType");
      if (withdrawFeeTypeIt != chainDetail.end()) {
        std::string_view withdrawFeeTypeStr = withdrawFeeTypeIt->get<std::string_view>();
        if (withdrawFeeTypeStr == "fixed") {
          std::string_view withdrawFeeStr = chainDetail["transactFeeWithdraw"].get<std::string_view>();
          MonetaryAmount withdrawFee(withdrawFeeStr, cur);
          log::trace("Retrieved {} withdrawal fee {}", _name, withdrawFee);
          fees.push_back(withdrawFee);
        } else if (withdrawFeeTypeStr == "rate") {
          log::debug("Unsupported rate withdraw fee for {}", _name);
          fees.emplace_back(0, cur);
        } else if (withdrawFeeTypeStr == "circulated") {
          log::debug("Unsupported circulated withdraw fee for {}", _name);
          fees.emplace_back(0, cur);
        }
      }

      foundChainWithSameName = true;
      break;
    }
    if (!foundChainWithSameName) {
      log::debug("Cannot find '{}' main chain in {}, discarding currency", curStr, _name);
    }
  }

  log::info("Retrieved {} withdrawal fees for {} coins", _name, fees.size());
  return MonetaryAmountByCurrencySet(std::move(fees));
}

std::optional<MonetaryAmount> HuobiPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
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
  return {};
}

MarketOrderBookMap HuobiPublic::AllOrderBooksFunc::operator()(int depth) {
  MarketOrderBookMap ret;
  const auto& [markets, marketInfoMap] = _marketsCache.get();
  using HuobiAssetPairToStdMarketMap = std::unordered_map<string, Market>;
  HuobiAssetPairToStdMarketMap huobiAssetPairToStdMarketMap;
  huobiAssetPairToStdMarketMap.reserve(markets.size());
  for (Market mk : markets) {
    huobiAssetPairToStdMarketMap.insert_or_assign(mk.assetsPairStrUpper(), mk);
  }
  const auto tickerData = PublicQuery(_curlHandle, "/market/tickers");
  const auto time = Clock::now();
  for (const json& tickerDetails : tickerData) {
    string upperMarket = ToUpper(tickerDetails["symbol"].get<std::string_view>());
    auto it = huobiAssetPairToStdMarketMap.find(upperMarket);
    if (it == huobiAssetPairToStdMarketMap.end()) {
      continue;
    }
    Market mk = it->second;
    const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(mk)->second;
    VolAndPriNbDecimals volAndPriNbDecimals = marketInfo.volAndPriNbDecimals;
    MonetaryAmount askPri(tickerDetails["ask"].get<double>(), mk.quote(), MonetaryAmount::RoundType::kNearest,
                          volAndPriNbDecimals.priNbDecimals);
    MonetaryAmount bidPri(tickerDetails["bid"].get<double>(), mk.quote(), MonetaryAmount::RoundType::kNearest,
                          volAndPriNbDecimals.priNbDecimals);
    MonetaryAmount askVol(tickerDetails["askSize"].get<double>(), mk.base(), MonetaryAmount::RoundType::kUp,
                          volAndPriNbDecimals.volNbDecimals);
    MonetaryAmount bidVol(tickerDetails["bidSize"].get<double>(), mk.base(), MonetaryAmount::RoundType::kUp,
                          volAndPriNbDecimals.volNbDecimals);

    if (bidVol == 0 || askVol == 0) {
      log::trace("No volume for {}", mk);
      continue;
    }

    ret.insert_or_assign(mk, MarketOrderBook(time, askPri, askVol, bidPri, bidVol, volAndPriNbDecimals, depth));
  }

  log::info("Retrieved Huobi ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook HuobiPublic::OrderBookFunc::operator()(Market mk, int depth) {
  // Huobi has a fixed range of authorized values for depth
  CurlPostData postData{{"symbol", mk.assetsPairStrLower()}, {"type", "step0"}};
  if (depth != kHuobiStandardOrderBookDefaultDepth) {
    static constexpr std::array kAuthorizedDepths = {5, 10, 20, kHuobiStandardOrderBookDefaultDepth};
    auto lb = std::ranges::lower_bound(kAuthorizedDepths, depth);
    if (lb == kAuthorizedDepths.end()) {
      log::warn("Invalid depth {}, default to {}", depth, kHuobiStandardOrderBookDefaultDepth);
    } else if (*lb != kHuobiStandardOrderBookDefaultDepth) {
      postData.append("depth", *lb);
    }
  }
  using OrderBookVec = vector<OrderBookLine>;
  OrderBookVec orderBookLines;

  const json asksAndBids = PublicQuery(_curlHandle, "/market/depth", postData);
  const auto asksIt = asksAndBids.find("asks");
  const auto bidsIt = asksAndBids.find("bids");
  if (asksIt != asksAndBids.end() && bidsIt != asksAndBids.end()) {
    orderBookLines.reserve(bidsIt->size() + asksIt->size());
    for (const auto& asksOrBids : {bidsIt, asksIt}) {
      int currentDepth = 0;
      const auto type = asksOrBids == asksIt ? OrderBookLine::Type::kAsk : OrderBookLine::Type::kBid;
      for (const auto& priceQuantityPair : *asksOrBids) {
        MonetaryAmount amount(priceQuantityPair.back().get<double>(), mk.base());
        MonetaryAmount price(priceQuantityPair.front().get<double>(), mk.quote());

        orderBookLines.emplace_back(amount, price, type);
        if (++currentDepth == depth) {
          if (depth < static_cast<int>(asksOrBids->size())) {
            log::debug("Truncate number of {} prices in order book to {}",
                       type == OrderBookLine::Type::kAsk ? "ask" : "bid", depth);
          }
          break;
        }
      }
    }
  }
  return MarketOrderBook(Clock::now(), mk, orderBookLines);
}

MonetaryAmount HuobiPublic::sanitizePrice(Market mk, MonetaryAmount pri) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  MonetaryAmount sanitizedPri = pri;
  auto priNbDecimals = marketInfoMap.find(mk)->second.volAndPriNbDecimals.priNbDecimals;
  MonetaryAmount minPri(1, pri.currencyCode(), priNbDecimals);
  if (sanitizedPri < minPri) {
    sanitizedPri = minPri;
  } else {
    sanitizedPri.truncate(priNbDecimals);
  }
  if (sanitizedPri != pri) {
    log::warn("Sanitize price {} -> {}", pri, sanitizedPri);
  }
  return sanitizedPri;
}

MonetaryAmount HuobiPublic::sanitizeVolume(Market mk, CurrencyCode fromCurrencyCode, MonetaryAmount vol,
                                           MonetaryAmount sanitizedPrice, bool isTakerOrder) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(mk)->second;
  MonetaryAmount sanitizedVol = vol;

  if (sanitizedVol.toNeutral() * sanitizedPrice < marketInfo.minOrderValue) {
    sanitizedVol = MonetaryAmount(marketInfo.minOrderValue / sanitizedPrice, sanitizedVol.currencyCode());
    sanitizedVol.round(marketInfo.volAndPriNbDecimals.volNbDecimals, MonetaryAmount::RoundType::kUp);
  } else {
    sanitizedVol.truncate(marketInfo.volAndPriNbDecimals.volNbDecimals);
  }
  if (isTakerOrder) {
    if (fromCurrencyCode == mk.base() && sanitizedVol < marketInfo.sellMarketMinOrderAmount) {
      sanitizedVol = marketInfo.sellMarketMinOrderAmount;
    }
  } else {
    if (sanitizedVol < marketInfo.limitMinOrderAmount) {
      sanitizedVol = marketInfo.limitMinOrderAmount;
    }
  }
  if (sanitizedVol != vol) {
    log::warn("Sanitize volume {} -> {}", vol, sanitizedVol);
  }
  return sanitizedVol;
}

MonetaryAmount HuobiPublic::TradedVolumeFunc::operator()(Market mk) {
  json result = PublicQuery(_curlHandle, "/market/detail/merged", {{"symbol", mk.assetsPairStrLower()}});
  const auto amountIt = result.find("amountIt");
  double last24hVol = amountIt == result.end() ? 0 : amountIt->get<double>();
  return MonetaryAmount(last24hVol, mk.base());
}

TradesVector HuobiPublic::queryLastTrades(Market mk, int nbTrades) {
  nbTrades = std::min(nbTrades, 2000);  // max authorized
  nbTrades = std::max(nbTrades, 1);     // min authorized
  json result =
      PublicQuery(_curlHandle, "/market/history/trade", {{"symbol", mk.assetsPairStrLower()}, {"size", nbTrades}});
  TradesVector ret;
  for (const json& detail : result) {
    auto dataDetails = detail.find("data");
    if (dataDetails != detail.end()) {
      for (const json& detail2 : *dataDetails) {
        MonetaryAmount amount(detail2["amount"].get<double>(), mk.base());
        MonetaryAmount price(detail2["price"].get<double>(), mk.quote());
        int64_t millisecondsSinceEpoch = detail2["ts"].get<int64_t>();
        TradeSide tradeSide =
            detail2["direction"].get<std::string_view>() == "buy" ? TradeSide::kBuy : TradeSide::kSell;

        ret.emplace_back(tradeSide, amount, price, TimePoint(milliseconds(millisecondsSinceEpoch)));
      }
    }
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount HuobiPublic::TickerFunc::operator()(Market mk) {
  json result = PublicQuery(_curlHandle, "/market/trade", {{"symbol", mk.assetsPairStrLower()}});
  const auto dataIt = result.find("data");
  double lastPrice = dataIt == result.end() ? 0 : dataIt->front()["price"].get<double>();
  return MonetaryAmount(lastPrice, mk.quote());
}
}  // namespace cct::api
