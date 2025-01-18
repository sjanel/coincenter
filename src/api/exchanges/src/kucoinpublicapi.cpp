#include "kucoinpublicapi.hpp"

#include <algorithm>
#include <amc/isdetected.hpp>
#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "cachedresult.hpp"
#include "cct_const.hpp"
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
#include "kucoin-schema.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetary-amount-vector.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "order-book-line.hpp"
#include "permanentcurloptions.hpp"
#include "public-trade-vector.hpp"
#include "request-retry.hpp"
#include "stringconv.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct::api {
namespace {

template <class T>
T PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, const CurlPostData& curlPostData = CurlPostData()) {
  RequestRetry requestRetry(curlHandle, CurlOptions(HttpRequestType::kGet, curlPostData));

  return requestRetry.query<T>(endpoint, [](const T& response) {
    if constexpr (amc::is_detected<schema::kucoin::has_code_t, T>::value) {
      if (response.code != KucoinPublic::kStatusCodeOK) {
        log::warn("Kucoin error: '{}'", response.code);
        return RequestRetry::Status::kResponseError;
      }
    }
    return RequestRetry::Status::kResponseOK;
  });
}

}  // namespace

KucoinPublic::KucoinPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CommonAPI& commonAPI)
    : ExchangePublic(ExchangeNameEnum::kucoin, fiatConverter, commonAPI, config),
      _curlHandle(kUrlBase, config.metricGatewayPtr(), permanentCurlOptionsBuilder().build(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeConfig().query.getUpdateFrequency(QueryType::currencies), _cachedResultVault),
          _curlHandle, _coincenterInfo, commonAPI),
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

bool KucoinPublic::healthCheck() {
  auto result = PublicQuery<schema::kucoin::V1Status>(_curlHandle, "/api/v1/status");
  log::info("{} status: {}, msg: {}", name(), result.data.status, result.data.msg);
  return result.data.status == "open";
}

KucoinPublic::TradableCurrenciesFunc::CurrencyInfoSet KucoinPublic::TradableCurrenciesFunc::operator()() {
  const auto result = PublicQuery<schema::kucoin::V3Currencies>(_curlHandle, "/api/v3/currencies");
  vector<CurrencyInfo> currencyInfos;
  currencyInfos.reserve(static_cast<uint32_t>(result.data.size()));
  for (const auto& curDetail : result.data) {
    std::string_view curStr = curDetail.currency;

    if (curStr.size() > CurrencyCode::kMaxLen) {
      log::warn("Discard {} as currency code is too long", curStr);
      continue;
    }

    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));

    if (!curDetail.chains) {
      CurrencyExchange currencyExchange(
          cur, curStr, curStr, CurrencyExchange::Deposit::kUnavailable, CurrencyExchange::Withdraw::kUnavailable,
          _commonApi.queryIsCurrencyCodeFiat(cur) ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto);

      log::debug("Retrieved Kucoin Currency {}", currencyExchange.str());

      currencyInfos.emplace_back(std::move(currencyExchange), MonetaryAmount(0, cur), MonetaryAmount(0, cur));
      continue;
    }

    const auto& chains = *curDetail.chains;

    bool foundChain = false;
    for (const auto& curChain : chains) {
      if (chains.size() > 1U && curChain.chainName != curStr &&
          (curChain.chainId.size() > CurrencyCode::kMaxLen || CurrencyCode(curChain.chainId) != cur)) {
        log::debug("Discard {} as chain name is different from currency code", curStr);
        continue;
      }

      foundChain = true;

      CurrencyExchange currencyExchange(
          cur, curStr, curStr,
          curChain.isDepositEnabled ? CurrencyExchange::Deposit::kAvailable : CurrencyExchange::Deposit::kUnavailable,
          curChain.isWithdrawEnabled ? CurrencyExchange::Withdraw::kAvailable
                                     : CurrencyExchange::Withdraw::kUnavailable,
          _commonApi.queryIsCurrencyCodeFiat(cur) ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto);

      log::debug("Retrieved Kucoin Currency {}", currencyExchange.str());

      currencyInfos.emplace_back(std::move(currencyExchange), MonetaryAmount(curChain.withdrawalMinSize, cur),
                                 MonetaryAmount(curChain.withdrawalMinFee, cur));
      break;
    }
    if (!foundChain) {
      log::debug("Discard {} as no chain name matches currency code", curStr);
    }
  }
  log::info("Retrieved {} Kucoin currencies", currencyInfos.size());
  return CurrencyInfoSet(std::move(currencyInfos));
}

CurrencyExchangeFlatSet KucoinPublic::queryTradableCurrencies() {
  const TradableCurrenciesFunc::CurrencyInfoSet& currencyInfoSet = _tradableCurrenciesCache.get();
  CurrencyExchangeVector currencies(currencyInfoSet.size());
  std::ranges::transform(currencyInfoSet, currencies.begin(),
                         [](const auto& currencyInfo) { return currencyInfo.currencyExchange; });
  return CurrencyExchangeFlatSet(std::move(currencies));
}

std::pair<MarketSet, KucoinPublic::MarketsFunc::MarketInfoMap> KucoinPublic::MarketsFunc::operator()() {
  auto result = PublicQuery<schema::kucoin::V2Symbols>(_curlHandle, "/api/v2/symbols");

  MarketSet markets;
  MarketInfoMap marketInfoMap;

  markets.reserve(static_cast<MarketSet::size_type>(result.data.size()));

  const CurrencyCodeSet& excludedCurrencies = _assetConfig.allExclude;

  for (const auto& marketDetails : result.data) {
    const std::string_view baseAsset = marketDetails.baseCurrency;
    const std::string_view quoteAsset = marketDetails.quoteCurrency;
    const bool isEnabled = marketDetails.enableTrading;
    if (!isEnabled) {
      log::trace("Trading is disabled for market {}-{}", baseAsset, quoteAsset);
      continue;
    }
    if (excludedCurrencies.contains(baseAsset) || excludedCurrencies.contains(quoteAsset)) {
      log::trace("Discard {}-{} excluded by config", baseAsset, quoteAsset);
      continue;
    }
    if (baseAsset.size() > CurrencyCode::kMaxLen || quoteAsset.size() > CurrencyCode::kMaxLen ||
        marketDetails.feeCurrency.size() > CurrencyCode::kMaxLen) {
      log::trace("Discard {}-{} as one asset is too long", baseAsset, quoteAsset);
      continue;
    }
    log::trace("Accept {}-{} Kucoin asset pair", baseAsset, quoteAsset);
    CurrencyCode base(baseAsset);
    CurrencyCode quote(quoteAsset);
    Market mk(base, quote);

    markets.insert(mk);

    MarketInfo& marketInfo = marketInfoMap[std::move(mk)];

    marketInfo.baseMinSize = MonetaryAmount(marketDetails.baseMinSize, base);
    marketInfo.quoteMinSize = MonetaryAmount(marketDetails.quoteMinSize, quote);
    marketInfo.baseMaxSize = MonetaryAmount(marketDetails.baseMaxSize, base);
    marketInfo.quoteMaxSize = MonetaryAmount(marketDetails.quoteMaxSize, quote);
    marketInfo.baseIncrement = MonetaryAmount(marketDetails.baseIncrement, base);
    marketInfo.priceIncrement = MonetaryAmount(marketDetails.priceIncrement, quote);
    marketInfo.feeCurrency = CurrencyCode(marketDetails.feeCurrency);
  }
  log::debug("Retrieved {} markets for kucoin", markets.size());
  return {std::move(markets), std::move(marketInfoMap)};
}

MonetaryAmountByCurrencySet KucoinPublic::queryWithdrawalFees() {
  MonetaryAmountVector fees;
  const auto& tradableCurrencies = _tradableCurrenciesCache.get();
  fees.reserve(tradableCurrencies.size());
  for (const TradableCurrenciesFunc::CurrencyInfo& curDetail : tradableCurrencies) {
    fees.push_back(curDetail.withdrawalMinFee);
    log::trace("Retrieved {} withdrawal fee {}", name(), curDetail.withdrawalMinFee);
  }

  log::info("Retrieved {} withdrawal fees for {} coins", name(), fees.size());
  return MonetaryAmountByCurrencySet(std::move(fees));
}

std::optional<MonetaryAmount> KucoinPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  const auto& currencyInfoSet = _tradableCurrenciesCache.get();
  auto it = currencyInfoSet.lower_bound(TradableCurrenciesFunc::CurrencyInfo(currencyCode));
  if (it == currencyInfoSet.end()) {
    return {};
  }
  return MonetaryAmount(it->withdrawalMinFee, it->currencyExchange.standardCode());
}

MarketOrderBookMap KucoinPublic::AllOrderBooksFunc::operator()(int depth) {
  MarketOrderBookMap ret;
  const auto& [markets, marketInfoMap] = _marketsCache.get();
  const auto data = PublicQuery<schema::kucoin::V1AllTickers>(_curlHandle, "/api/v1/market/allTickers");
  const auto time = Clock::now();
  for (const auto& ticker : data.data.ticker) {
    if (ticker.symbol.size() > Market::kMaxLen) {
      log::debug("Discarding {} because of invalid ticker size", ticker.symbol);
      continue;
    }
    Market mk(ticker.symbol, '-');
    if (!markets.contains(mk)) {
      log::debug("Market {} is not present", mk);
      continue;
    }
    MonetaryAmount askPri(ticker.sell.value_or(MonetaryAmount()), mk.quote());
    if (askPri == 0) {
      log::debug("Discarding {} because of invalid ask price {}", mk, askPri);
      continue;
    }
    MonetaryAmount bidPri(ticker.buy.value_or(MonetaryAmount()), mk.quote());
    if (bidPri == 0) {
      log::debug("Discarding {} because of invalid bid price {}", mk, bidPri);
      continue;
    }
    // There is no volume in the response, we need to emulate it, based on the 24h volume
    MonetaryAmount dayVolume(ticker.vol.value_or(MonetaryAmount()), mk.base());
    if (dayVolume == 0) {
      log::debug("Discarding {} because of invalid volume {}", mk, dayVolume);
      continue;
    }
    const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(mk)->second;

    // Use avg traded volume by second as ask/bid vol
    static constexpr MonetaryAmount::AmountType kAskVolPerDay = static_cast<MonetaryAmount::AmountType>(2) * 24 * 3600;

    MonetaryAmount askVol = dayVolume / kAskVolPerDay;
    askVol.round(marketInfo.baseIncrement, MonetaryAmount::RoundType::kNearest);
    if (askVol == 0) {
      askVol = marketInfo.baseIncrement;
    }
    MonetaryAmount bidVol = askVol;

    VolAndPriNbDecimals volAndPriNbDecimals{marketInfo.baseIncrement.nbDecimals(),
                                            marketInfo.priceIncrement.nbDecimals()};

    ret.insert_or_assign(mk, MarketOrderBook(time, askPri, askVol, bidPri, bidVol, volAndPriNbDecimals, depth));
  }

  log::info("Retrieved Kucoin ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook KucoinPublic::OrderBookFunc::operator()(Market mk, int depth) {
  // Kucoin has a fixed range of authorized values for depth
  static constexpr std::array kAuthorizedDepths = {20, 100};
  auto lb = std::ranges::lower_bound(kAuthorizedDepths, depth);
  if (lb == kAuthorizedDepths.end()) {
    lb = std::next(kAuthorizedDepths.end(), -1);
    log::warn("Invalid depth {}, default to {}", depth, *lb);
  }

  string endpoint("/api/v1/market/orderbook/level2_");
  AppendIntegralToString(endpoint, *lb);

  MarketOrderBookLines orderBookLines;

  const auto asksAndBids = PublicQuery<schema::kucoin::V1PartOrderBook>(_curlHandle, endpoint, GetSymbolPostData(mk));
  const auto nowTime = Clock::now();

  if (asksAndBids.data.asks.size() == asksAndBids.data.bids.size()) {
    orderBookLines.reserve(std::min(static_cast<decltype(depth)>(asksAndBids.data.asks.size()), depth) +
                           std::min(static_cast<decltype(depth)>(asksAndBids.data.bids.size()), depth));

    // Reverse iterate as bids are received in descending order
    for (const auto& val : asksAndBids.data.bids | std::views::reverse | std::ranges::views::take(depth)) {
      MonetaryAmount price(val[0], mk.quote());
      MonetaryAmount amount(val[1], mk.base());

      orderBookLines.pushBid(amount, price);
    }

    for (const auto& val : asksAndBids.data.asks | std::ranges::views::take(depth)) {
      MonetaryAmount price(val[0], mk.quote());
      MonetaryAmount amount(val[1], mk.base());

      orderBookLines.pushAsk(amount, price);
    }
  } else {
    log::error("Unexpected Kucoin order book response - number of asks != number of bids {} != {}",
               asksAndBids.data.asks.size(), asksAndBids.data.bids.size());
  }

  return MarketOrderBook(nowTime, mk, orderBookLines);
}

MonetaryAmount KucoinPublic::sanitizePrice(Market mk, MonetaryAmount pri) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(mk)->second;
  MonetaryAmount sanitizedPri = pri;
  if (pri < marketInfo.priceIncrement) {
    sanitizedPri = marketInfo.priceIncrement;
  } else {
    sanitizedPri.round(marketInfo.priceIncrement, MonetaryAmount::RoundType::kNearest);
  }
  if (sanitizedPri != pri) {
    log::debug("Sanitize price {} -> {}", pri, sanitizedPri);
  }
  return sanitizedPri;
}

MonetaryAmount KucoinPublic::sanitizeVolume(Market mk, MonetaryAmount vol) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(mk)->second;
  MonetaryAmount sanitizedVol = vol;
  // TODO: Kucoin documentation is not clear about this, this would probably need to be adjusted
  if (sanitizedVol < marketInfo.baseMinSize) {
    sanitizedVol = marketInfo.baseMinSize;
  } else if (sanitizedVol > marketInfo.baseMaxSize) {
    sanitizedVol = marketInfo.baseMaxSize;
  } else {
    sanitizedVol.round(marketInfo.baseIncrement, MonetaryAmount::RoundType::kDown);
  }
  if (sanitizedVol != vol) {
    log::debug("Sanitize volume {} -> {}", vol, sanitizedVol);
  }
  return sanitizedVol;
}

MonetaryAmount KucoinPublic::TradedVolumeFunc::operator()(Market mk) {
  const auto result =
      PublicQuery<schema::kucoin::V1MarketStats>(_curlHandle, "/api/v1/market/stats", GetSymbolPostData(mk));
  return {result.data.vol, mk.base()};
}

PublicTradeVector KucoinPublic::queryLastTrades(Market mk, int nbTrades) {
  static constexpr auto kMaxNbLastTrades = 100;
  if (nbTrades > kMaxNbLastTrades) {
    log::warn("Maximum number of last trades to query from {} is {}", name(), kMaxNbLastTrades);
  }

  auto result =
      PublicQuery<schema::kucoin::V1MarketHistories>(_curlHandle, "/api/v1/market/histories", GetSymbolPostData(mk));

  PublicTradeVector ret;
  ret.reserve(std::min(static_cast<PublicTradeVector::size_type>(result.data.size()),
                       static_cast<PublicTradeVector::size_type>(nbTrades)));

  for (const auto& detail : result.data | std::ranges::views::take(nbTrades)) {
    const MonetaryAmount amount(detail.size, mk.base());
    const MonetaryAmount price(detail.price, mk.quote());
    // time is in nanoseconds
    const int64_t millisecondsSinceEpoch = static_cast<int64_t>(detail.time / 1000000UL);
    const TradeSide tradeSide =
        detail.side == schema::kucoin::V1MarketHistories::V1MarketHistory::Side::buy ? TradeSide::buy : TradeSide::sell;

    ret.emplace_back(tradeSide, amount, price, TimePoint(milliseconds(millisecondsSinceEpoch)));
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount KucoinPublic::TickerFunc::operator()(Market mk) {
  const auto result = PublicQuery<schema::kucoin::V1MarketOrderbookLevel1>(
      _curlHandle, "/api/v1/market/orderbook/level1", GetSymbolPostData(mk));
  return {result.data.price, mk.quote()};
}
}  // namespace cct::api
