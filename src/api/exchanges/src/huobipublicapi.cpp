
#include "huobipublicapi.hpp"

#include <algorithm>
#include <amc/isdetected.hpp>
#include <array>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currency-chain-picker.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchange-asset-config.hpp"
#include "exchange-name-enum.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "httprequesttype.hpp"
#include "huobi-schema.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetary-amount-vector.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "order-book-line.hpp"
#include "permanentcurloptions.hpp"
#include "public-trade-vector.hpp"
#include "read-json.hpp"
#include "request-retry.hpp"
#include "timedef.hpp"
#include "toupperlower-string.hpp"
#include "tradeside.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct::api {
namespace {

constexpr std::string_view kHealthCheckBaseUrl[] = {"https://status.huobigroup.com"};

template <class T>
T PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, const CurlPostData& curlPostData = CurlPostData()) {
  string method(endpoint);
  if (!curlPostData.empty()) {
    method.push_back('?');
    method.append(curlPostData.str());
  }

  RequestRetry requestRetry(curlHandle, CurlOptions(HttpRequestType::kGet));
  return requestRetry.query<T>(method, [](const T& response) {
    if constexpr (amc::is_detected<schema::huobi::has_code_t, T>::value) {
      if (response.code != 200) {
        log::warn("Huobi error code: {}", response.code);
        return RequestRetry::Status::kResponseError;
      }
    } else if constexpr (amc::is_detected<schema::huobi::has_status_t, T>::value) {
      if (response.status != "ok") {
        log::warn("Huobi status error: {}", response.status);
        return RequestRetry::Status::kResponseError;
      }
    } else {
      // TODO: can be replaced by static_assert(false) in C++23
      static_assert(amc::is_detected<schema::huobi::has_code_t, T>::value ||
                        amc::is_detected<schema::huobi::has_status_t, T>::value,
                    "T should have a code or status member");
    }

    return RequestRetry::Status::kResponseOK;
  });
}

}  // namespace

HuobiPublic::HuobiPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CommonAPI& commonAPI)
    : ExchangePublic(ExchangeNameEnum::huobi, fiatConverter, commonAPI, config),
      _curlHandle(kURLBases, config.metricGatewayPtr(), permanentCurlOptionsBuilder().build(), config.getRunMode()),
      _healthCheckCurlHandle(kHealthCheckBaseUrl, config.metricGatewayPtr(),
                             PermanentCurlOptions::Builder()
                                 .setMinDurationBetweenQueries(exchangeConfig().query.publicAPIRate.duration)
                                 .build(),
                             config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeConfig().query.getUpdateFrequency(QueryType::currencies), _cachedResultVault),
          _curlHandle),
      _marketsCache(
          CachedResultOptions(exchangeConfig().query.getUpdateFrequency(QueryType::markets), _cachedResultVault),
          _curlHandle, exchangeConfig().asset),
      _allOrderBooksCache(
          CachedResultOptions(exchangeConfig().query.getUpdateFrequency(QueryType::allOrderBooks), _cachedResultVault),
          _marketsCache, _curlHandle),
      _orderbookCache(
          CachedResultOptions(exchangeConfig().query.getUpdateFrequency(QueryType::orderBook), _cachedResultVault),
          _curlHandle),
      _tradedVolumeCache(
          CachedResultOptions(exchangeConfig().query.getUpdateFrequency(QueryType::tradedVolume), _cachedResultVault),
          _curlHandle),
      _tickerCache(
          CachedResultOptions(exchangeConfig().query.getUpdateFrequency(QueryType::lastPrice), _cachedResultVault),
          _curlHandle) {}

bool HuobiPublic::healthCheck() {
  auto strData = _healthCheckCurlHandle.query("/api/v2/summary.json", CurlOptions(HttpRequestType::kGet));
  schema::huobi::V2SystemStatus networkInfo;
  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = ReadJson<json::opts{.error_on_unknown_keys = false, .minified = true, .raw_string = true}>(
      strData, "Huobi system status", networkInfo);
  if (ec) {
    log::error("{} health check response is badly formatted", name());
    return false;
  }
  std::string_view statusStr = networkInfo.status.description;
  log::info("{} status: {}", name(), statusStr);
  return networkInfo.incidents.empty();
}

schema::huobi::V2ReferenceCurrency HuobiPublic::TradableCurrenciesFunc::operator()() {
  return PublicQuery<schema::huobi::V2ReferenceCurrency>(_curlHandle, "/v2/reference/currencies");
}

namespace {
CurrencyChainPicker<schema::huobi::V2ReferenceCurrencyDetails::Chain> CreateCurrencyChainPicker(
    const schema::ExchangeAssetConfig& assetConfig) {
  return {assetConfig, [](const schema::huobi::V2ReferenceCurrencyDetails::Chain& chain) -> std::string_view {
            return chain.displayName;
          }};
}
}  // namespace

HuobiPublic::WithdrawParams HuobiPublic::getWithdrawParams(CurrencyCode cur) {
  WithdrawParams withdrawParams;
  const auto& assetConfig = _coincenterInfo.exchangeConfig(exchangeNameEnum()).asset;
  const auto currencyChainPicker = CreateCurrencyChainPicker(assetConfig);
  for (const auto& curDetail : _tradableCurrenciesCache.get().data) {
    if (cur == CurrencyCode(_coincenterInfo.standardizeCurrencyCode(curDetail.currency))) {
      for (const auto& chainDetail : curDetail.chains) {
        if (currencyChainPicker.shouldDiscardChain(curDetail.chains, cur, chainDetail)) {
          continue;
        }

        withdrawParams.minWithdrawAmt = MonetaryAmount(chainDetail.minWithdrawAmt, cur);
        withdrawParams.maxWithdrawAmt = MonetaryAmount(chainDetail.maxWithdrawAmt, cur);
        withdrawParams.withdrawPrecision = chainDetail.withdrawPrecision;

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
  const auto& assetConfig = _coincenterInfo.exchangeConfig(exchangeNameEnum()).asset;
  const auto currencyChainPicker = CreateCurrencyChainPicker(assetConfig);
  for (const auto& curDetail : _tradableCurrenciesCache.get().data) {
    std::string_view statusStr = curDetail.instStatus;
    std::string_view curStr = curDetail.currency;
    if (statusStr != "normal") {
      log::debug("{} is {} from Huobi", curStr, statusStr);
      continue;
    }
    bool foundChainWithSameName = false;
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    for (const auto& chainDetail : curDetail.chains) {
      if (currencyChainPicker.shouldDiscardChain(curDetail.chains, cur, chainDetail)) {
        continue;
      }
      auto depositAllowed = chainDetail.depositStatus == "allowed" ? CurrencyExchange::Deposit::kAvailable
                                                                   : CurrencyExchange::Deposit::kUnavailable;
      auto withdrawAllowed = chainDetail.withdrawStatus == "allowed" ? CurrencyExchange::Withdraw::kAvailable
                                                                     : CurrencyExchange::Withdraw::kUnavailable;
      auto curExchangeType =
          _commonApi.queryIsCurrencyCodeFiat(cur) ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto;

      CurrencyExchange newCurrency(cur, curStr, curStr, depositAllowed, withdrawAllowed, curExchangeType);

      log::debug("Retrieved Huobi Currency {}", curStr);
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
  auto result =
      PublicQuery<schema::huobi::V1SettingsCommonMarketSymbols>(_curlHandle, "/v1/settings/common/market-symbols");

  MarketSet markets;
  MarketInfoMap marketInfoMap;

  markets.reserve(static_cast<MarketSet::size_type>(result.data.size()));

  const CurrencyCodeSet& excludedCurrencies = _assetConfig.allExclude;

  for (const auto& symbol : result.data) {
    std::string_view baseAsset = symbol.bc;
    std::string_view quoteAsset = symbol.qc;
    if (excludedCurrencies.contains(baseAsset) || excludedCurrencies.contains(quoteAsset)) {
      log::trace("Discard {}-{} excluded by config", baseAsset, quoteAsset);
      continue;
    }
    if (symbol.at != "enabled") {
      log::trace("Trading is {} for market {}-{}", symbol.at, baseAsset, quoteAsset);
      continue;
    }
    std::string_view stateStr = symbol.state;
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

    int8_t volNbDec = symbol.ap;
    int8_t priNbDec = symbol.pp;
    // value = vol * pri in quote currency
    MarketInfo marketInfo;

    marketInfo.volAndPriNbDecimals = {volNbDec, priNbDec};

    marketInfo.minOrderValue = MonetaryAmount(symbol.minov, quote);
    marketInfo.maxOrderValueUSDT = MonetaryAmount(symbol.maxov, quote);

    marketInfo.limitMinOrderAmount = MonetaryAmount(symbol.lominoa, base);
    marketInfo.limitMaxOrderAmount = MonetaryAmount(symbol.lomaxoa, base);

    marketInfo.sellMarketMinOrderAmount = MonetaryAmount(symbol.smminoa, base);
    marketInfo.sellMarketMaxOrderAmount = MonetaryAmount(symbol.smmaxoa, base);

    marketInfo.buyMarketMaxOrderValue = MonetaryAmount(symbol.bmmaxov, quote);

    marketInfoMap.insert_or_assign(mk, std::move(marketInfo));
  }
  log::info("Retrieved {} markets from huobi", markets.size());
  return {std::move(markets), std::move(marketInfoMap)};
}

MonetaryAmountByCurrencySet HuobiPublic::queryWithdrawalFees() {
  MonetaryAmountVector fees;
  const auto& assetConfig = _coincenterInfo.exchangeConfig(exchangeNameEnum()).asset;
  const auto currencyChainPicker = CreateCurrencyChainPicker(assetConfig);
  for (const auto& curDetail : _tradableCurrenciesCache.get().data) {
    std::string_view curStr = curDetail.currency;
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    bool foundChainWithSameName = false;
    for (const auto& chainDetail : curDetail.chains) {
      if (currencyChainPicker.shouldDiscardChain(curDetail.chains, cur, chainDetail)) {
        continue;
      }
      std::string_view withdrawFeeTypeStr = chainDetail.withdrawFeeType;
      if (withdrawFeeTypeStr == "fixed") {
        std::string_view withdrawFeeStr = chainDetail.transactFeeWithdraw;
        MonetaryAmount withdrawFee(withdrawFeeStr, cur);
        log::trace("Retrieved {} withdrawal fee {}", name(), withdrawFee);
        fees.push_back(withdrawFee);
      } else if (withdrawFeeTypeStr == "rate") {
        log::debug("Unsupported rate withdraw fee for {}", name());
        fees.emplace_back(0, cur);
      } else if (withdrawFeeTypeStr == "circulated") {
        log::debug("Unsupported circulated withdraw fee for {}", name());
        fees.emplace_back(0, cur);
      }

      foundChainWithSameName = true;
      break;
    }
    if (!foundChainWithSameName) {
      log::debug("Cannot find '{}' main chain in {}, discarding currency", curStr, name());
    }
  }

  log::info("Retrieved {} withdrawal fees for {} coins", name(), fees.size());
  return MonetaryAmountByCurrencySet(std::move(fees));
}

std::optional<MonetaryAmount> HuobiPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  for (const auto& curDetail : _tradableCurrenciesCache.get().data) {
    std::string_view curStr = curDetail.currency;
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    if (cur != currencyCode) {
      continue;
    }
    for (const auto& chainDetail : curDetail.chains) {
      std::string_view chainName = chainDetail.chain;
      if (chainName == cur) {
        return MonetaryAmount(chainDetail.transactFeeWithdraw, cur);
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
  const auto tickerData = PublicQuery<schema::huobi::MarketTickers>(_curlHandle, "/market/tickers");
  const auto time = Clock::now();
  for (const auto& tickerDetails : tickerData.data) {
    string upperMarket = ToUpper(tickerDetails.symbol);
    auto it = huobiAssetPairToStdMarketMap.find(upperMarket);
    if (it == huobiAssetPairToStdMarketMap.end()) {
      continue;
    }
    Market mk = it->second;
    const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(mk)->second;
    VolAndPriNbDecimals volAndPriNbDecimals = marketInfo.volAndPriNbDecimals;
    MonetaryAmount askPri(tickerDetails.ask, mk.quote(), MonetaryAmount::RoundType::kNearest,
                          volAndPriNbDecimals.priNbDecimals);
    MonetaryAmount bidPri(tickerDetails.bid, mk.quote(), MonetaryAmount::RoundType::kNearest,
                          volAndPriNbDecimals.priNbDecimals);
    MonetaryAmount askVol(tickerDetails.askSize, mk.base(), MonetaryAmount::RoundType::kUp,
                          volAndPriNbDecimals.volNbDecimals);
    MonetaryAmount bidVol(tickerDetails.bidSize, mk.base(), MonetaryAmount::RoundType::kUp,
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
    const auto lb = std::ranges::lower_bound(kAuthorizedDepths, depth);
    if (lb == kAuthorizedDepths.end()) {
      log::warn("Invalid depth {}, default to {}", depth, kHuobiStandardOrderBookDefaultDepth);
    } else if (*lb != kHuobiStandardOrderBookDefaultDepth) {
      postData.emplace_back("depth", *lb);
    }
  }

  MarketOrderBookLines orderBookLines;

  const auto ticks = PublicQuery<schema::huobi::MarketDepth>(_curlHandle, "/market/depth", postData);
  const auto nowTime = Clock::now();

  orderBookLines.reserve(std::min(static_cast<decltype(depth)>(ticks.tick.asks.size()), depth) +
                         std::min(static_cast<decltype(depth)>(ticks.tick.bids.size()), depth));
  for (const auto* asksOrBids : {&ticks.tick.asks, &ticks.tick.bids}) {
    const auto type = asksOrBids == &ticks.tick.asks ? OrderBookLine::Type::kAsk : OrderBookLine::Type::kBid;
    for (const auto& priceQuantityPair : *asksOrBids | std::ranges::views::take(depth)) {
      MonetaryAmount amount(priceQuantityPair[1], mk.base());
      MonetaryAmount price(priceQuantityPair[0], mk.quote());

      orderBookLines.push(amount, price, type);
    }
  }
  return MarketOrderBook(nowTime, mk, orderBookLines);
}

MonetaryAmount HuobiPublic::sanitizePrice(Market mk, MonetaryAmount pri) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  MonetaryAmount sanitizedPri = pri;
  auto marketIt = marketInfoMap.find(mk);
  if (marketIt == marketInfoMap.end()) {
    log::error("Unable to find market info for {} in sanitize price", mk);
    return sanitizedPri;
  }
  auto priNbDecimals = marketIt->second.volAndPriNbDecimals.priNbDecimals;
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
  auto marketIt = marketInfoMap.find(mk);
  if (marketIt == marketInfoMap.end()) {
    log::error("Unable to find market info for {} in sanitize volume", mk);
    return sanitizedPrice;
  }
  const MarketsFunc::MarketInfo& marketInfo = marketIt->second;
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
  const auto result = PublicQuery<schema::huobi::MarketDetailMerged>(_curlHandle, "/market/detail/merged",
                                                                     {{"symbol", mk.assetsPairStrLower()}});
  return MonetaryAmount(result.tick.amount, mk.base());
}

PublicTradeVector HuobiPublic::queryLastTrades(Market mk, int nbTrades) {
  static constexpr auto kNbMinLastTrades = 1;
  static constexpr auto kNbMaxLastTrades = 2000;

  if (nbTrades < kNbMinLastTrades) {
    log::warn("Minimum number of last trades to ask on {} is {}", name(), kNbMinLastTrades);
    nbTrades = kNbMinLastTrades;
  } else if (nbTrades > kNbMaxLastTrades) {
    log::warn("Maximum number of last trades to ask on {} is {}", name(), kNbMaxLastTrades);
    nbTrades = kNbMaxLastTrades;
  }

  auto result = PublicQuery<schema::huobi::MarketHistoryTrade>(
      _curlHandle, "/market/history/trade", {{"symbol", mk.assetsPairStrLower()}, {"size", nbTrades}});

  PublicTradeVector ret;
  ret.reserve(nbTrades);

  for (const auto& detail : result.data) {
    for (const auto& detail2 : detail.data) {
      const MonetaryAmount amount(detail2.amount, mk.base());
      const MonetaryAmount price(detail2.price, mk.quote());
      const int64_t millisecondsSinceEpoch = detail2.ts;
      const TradeSide tradeSide =
          detail2.direction == schema::huobi::MarketHistoryTrade::Trade::TradeData::Direction::buy ? TradeSide::buy
                                                                                                   : TradeSide::sell;

      ret.emplace_back(tradeSide, amount, price, TimePoint(milliseconds(millisecondsSinceEpoch)));

      if (static_cast<int>(ret.size()) == nbTrades) {
        break;
      }
    }
    if (static_cast<int>(ret.size()) == nbTrades) {
      break;
    }
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount HuobiPublic::TickerFunc::operator()(Market mk) {
  const auto result =
      PublicQuery<schema::huobi::MarketTrade>(_curlHandle, "/market/trade", {{"symbol", mk.assetsPairStrLower()}});
  double lastPrice = result.tick.data.empty() ? 0 : result.tick.data.front().price;
  return MonetaryAmount(lastPrice, mk.quote());
}
}  // namespace cct::api
