#include "kucoinpublicapi.hpp"

#include <algorithm>
#include <cassert>
#include <execution>
#include <thread>

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "curloptions.hpp"
#include "fiatconverter.hpp"
#include "monetaryamount.hpp"
#include "stringhelpers.hpp"
#include "toupperlower.hpp"

namespace cct::api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, const CurlPostData& curlPostData = CurlPostData()) {
  string method(endpoint);
  if (!curlPostData.empty()) {
    method.push_back('?');
    method.append(curlPostData.str());
  }
  json ret = json::parse(curlHandle.query(method, CurlOptions(HttpRequestType::kGet, KucoinPublic::kUserAgent)));
  auto errorIt = ret.find("code");
  if (errorIt != ret.end() && errorIt->get<std::string_view>() != "200000") {
    log::error("Full Kucoin json error: '{}'", ret.dump());
    throw exception("Kucoin error: {}", errorIt->get<std::string_view>());
  }
  return ret["data"];
}

}  // namespace

KucoinPublic::KucoinPublic(const CoincenterInfo& config, FiatConverter& fiatConverter,
                           api::CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("kucoin", fiatConverter, cryptowatchAPI, config),
      _curlHandle(kUrlBase, config.metricGatewayPtr(), exchangeInfo().publicAPIRate(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), _curlHandle,
          _coincenterInfo, cryptowatchAPI),
      _marketsCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kMarkets), _cachedResultVault),
                    _curlHandle, exchangeInfo()),
      _allOrderBooksCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kAllOrderBooks), _cachedResultVault),
          _marketsCache, _curlHandle, exchangeInfo()),
      _orderbookCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kOrderBook), _cachedResultVault),
                      _curlHandle, exchangeInfo()),
      _tradedVolumeCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kTradedVolume), _cachedResultVault),
          _curlHandle),
      _tickerCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kLastPrice), _cachedResultVault),
                   _curlHandle) {}

bool KucoinPublic::healthCheck() {
  json result =
      json::parse(_curlHandle.query("/api/v1/status", CurlOptions(HttpRequestType::kGet, KucoinPublic::kUserAgent)));
  auto dataIt = result.find("data");
  if (dataIt == result.end()) {
    log::error("Unexpected answer from {} status: {}", _name, result.dump());
    return false;
  }
  auto statusIt = dataIt->find("status");
  if (statusIt == dataIt->end()) {
    log::error("Unexpected answer from {} status: {}", _name, dataIt->dump());
    return false;
  }
  std::string_view statusStr = statusIt->get<std::string_view>();
  log::info("{} status: {}", _name, statusStr);
  return statusStr == "open";
}

KucoinPublic::TradableCurrenciesFunc::CurrencyInfoSet KucoinPublic::TradableCurrenciesFunc::operator()() {
  json result = PublicQuery(_curlHandle, "/api/v1/currencies");
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
  json result = PublicQuery(_curlHandle, "/api/v1/symbols");

  MarketSet markets;
  MarketInfoMap marketInfoMap;

  markets.reserve(static_cast<MarketSet::size_type>(result.size()));

  const CurrencyCodeSet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();

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
    if (baseAsset.size() > CurrencyCode::kMaxLen || quoteAsset.size() > CurrencyCode::kMaxLen) {
      log::trace("Discard {}-{} as one asset is too long", baseAsset, quoteAsset);
      continue;
    }
    log::trace("Accept {}-{} Kucoin asset pair", baseAsset, quoteAsset);
    CurrencyCode base(baseAsset);
    CurrencyCode quote(quoteAsset);
    Market mk(base, quote);
    markets.insert(mk);

    MarketInfo& marketInfo = marketInfoMap[std::move(mk)];

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

WithdrawalFeeMap KucoinPublic::queryWithdrawalFees() {
  WithdrawalFeeMap ret;
  for (const TradableCurrenciesFunc::CurrencyInfo& curDetail : _tradableCurrenciesCache.get()) {
    ret.insert_or_assign(curDetail.currencyExchange.standardCode(), curDetail.withdrawalMinFee);
    log::trace("Retrieved {} withdrawal fee {}", _name, curDetail.withdrawalMinFee);
  }

  log::info("Retrieved {} withdrawal fees for {} coins", _name, ret.size());
  assert(!ret.empty());
  return ret;
}

MonetaryAmount KucoinPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  const auto& currencyInfoSet = _tradableCurrenciesCache.get();
  auto it = currencyInfoSet.find(TradableCurrenciesFunc::CurrencyInfo(currencyCode));
  if (it == currencyInfoSet.end()) {
    throw exception("Unable to find withdrawal fee for {}", currencyCode);
  }
  return {it->withdrawalMinFee, it->currencyExchange.standardCode()};
}

MarketOrderBookMap KucoinPublic::AllOrderBooksFunc::operator()(int depth) {
  MarketOrderBookMap ret;
  const auto& [markets, marketInfoMap] = _marketsCache.get();
  json data = PublicQuery(_curlHandle, "/api/v1/market/allTickers");
  for (const json& tickerDetails : data["ticker"]) {
    Market mk(tickerDetails["symbol"].get<std::string_view>(), '-');
    if (!markets.contains(mk)) {
      log::trace("Market {} is not present", mk);
      continue;
    }
    MonetaryAmount askPri(tickerDetails["sell"].get<std::string_view>(), mk.quote());
    MonetaryAmount bidPri(tickerDetails["buy"].get<std::string_view>(), mk.quote());
    // There is no volume in the response, we need to emulate it, based on the 24h volume
    MonetaryAmount dayVolume(tickerDetails["vol"].get<std::string_view>(), mk.base());
    if (dayVolume == 0) {
      log::trace("No volume for {}", mk);
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

    ret.insert_or_assign(mk, MarketOrderBook(askPri, askVol, bidPri, bidVol, volAndPriNbDecimals, depth));
  }

  log::info("Retrieved Kucoin ticker information from {} markets", ret.size());
  return ret;
}

namespace {
template <class InputIt>
void FillOrderBook(Market mk, int depth, bool isAsk, InputIt beg, InputIt end, vector<OrderBookLine>& orderBookLines) {
  int currentDepth = 0;
  for (auto it = beg; it != end; ++it) {
    MonetaryAmount price((*it)[0].template get<std::string_view>(), mk.quote());
    MonetaryAmount amount((*it)[1].template get<std::string_view>(), mk.base());

    orderBookLines.emplace_back(amount, price, isAsk);
    if (++currentDepth == depth) {
      if (++it != end) {
        log::debug("Truncate number of {} prices in order book to {}", isAsk ? "ask" : "bid", depth);
      }
      break;
    }
  }
}
}  // namespace

MarketOrderBook KucoinPublic::OrderBookFunc::operator()(Market mk, int depth) {
  // Kucoin has a fixed range of authorized values for depth
  static constexpr int kAuthorizedDepths[] = {20, 100};
  auto lb = std::ranges::lower_bound(kAuthorizedDepths, depth);
  if (lb == std::end(kAuthorizedDepths)) {
    lb = std::next(std::end(kAuthorizedDepths), -1);
    log::warn("Invalid depth {}, default to {}", depth, *lb);
  }

  string endpoint("/api/v1/market/orderbook/level2_");
  AppendString(endpoint, *lb);

  json asksAndBids = PublicQuery(_curlHandle, endpoint, GetSymbolPostData(mk));
  const json& asks = asksAndBids["asks"];
  const json& bids = asksAndBids["bids"];
  using OrderBookVec = vector<OrderBookLine>;
  OrderBookVec orderBookLines;
  orderBookLines.reserve(static_cast<OrderBookVec::size_type>(depth) * 2);
  for (auto asksOrBids : {std::addressof(bids), std::addressof(asks)}) {
    const bool isAsk = asksOrBids == std::addressof(asks);
    if (isAsk) {
      FillOrderBook(mk, depth, isAsk, asksOrBids->begin(), asksOrBids->end(), orderBookLines);
    } else {
      // Reverse iterate as they are received in descending order
      FillOrderBook(mk, depth, isAsk, asksOrBids->rbegin(), asksOrBids->rend(), orderBookLines);
    }
  }
  return MarketOrderBook(mk, orderBookLines);
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
  json result = PublicQuery(_curlHandle, "/api/v1/market/stats", GetSymbolPostData(mk));
  return {result["vol"].get<std::string_view>(), mk.base()};
}

LastTradesVector KucoinPublic::queryLastTrades(Market mk, [[maybe_unused]] int nbTrades) {
  json result = PublicQuery(_curlHandle, "/api/v1/market/histories", GetSymbolPostData(mk));
  LastTradesVector ret;
  ret.reserve(static_cast<LastTradesVector::size_type>(result.size()));
  for (const json& detail : result) {
    MonetaryAmount amount(detail["size"].get<std::string_view>(), mk.base());
    MonetaryAmount price(detail["price"].get<std::string_view>(), mk.quote());
    // time is in nanoseconds
    int64_t millisecondsSinceEpoch = static_cast<int64_t>(detail["time"].get<uintmax_t>() / 1000000UL);
    TradeSide tradeSide = detail["side"].get<std::string_view>() == "buy" ? TradeSide::kBuy : TradeSide::kSell;

    ret.emplace_back(tradeSide, amount, price, TimePoint(std::chrono::milliseconds(millisecondsSinceEpoch)));
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount KucoinPublic::TickerFunc::operator()(Market mk) {
  json result = PublicQuery(_curlHandle, "/api/v1/market/orderbook/level1", GetSymbolPostData(mk));
  return {result["price"].get<std::string_view>(), mk.quote()};
}
}  // namespace cct::api
