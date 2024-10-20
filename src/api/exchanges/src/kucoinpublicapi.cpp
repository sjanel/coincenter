#include "kucoinpublicapi.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "cachedresult.hpp"
#include "cct_json-container.hpp"
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

json::container PublicQuery(CurlHandle& curlHandle, std::string_view endpoint,
                            const CurlPostData& curlPostData = CurlPostData()) {
  RequestRetry requestRetry(curlHandle, CurlOptions(HttpRequestType::kGet, curlPostData));

  json::container jsonResponse = requestRetry.queryJson(endpoint, [](const json::container& jsonResponse) {
    const auto errorIt = jsonResponse.find("code");
    if (errorIt != jsonResponse.end() && errorIt->get<std::string_view>() != "200000") {
      log::warn("Full Kucoin error ({}): '{}'", errorIt->get<std::string_view>(), jsonResponse.dump());
      return RequestRetry::Status::kResponseError;
    }
    return RequestRetry::Status::kResponseOK;
  });
  json::container ret;
  const auto dataIt = jsonResponse.find("data");
  if (dataIt != jsonResponse.end()) {
    ret.swap(*dataIt);
  }
  return ret;
}

}  // namespace

KucoinPublic::KucoinPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CommonAPI& commonAPI)
    : ExchangePublic("kucoin", fiatConverter, commonAPI, config),
      _curlHandle(kUrlBase, config.metricGatewayPtr(),
                  exchangeConfig().curlOptionsBuilderBase(ExchangeConfig::Api::kPublic).build(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), _curlHandle,
          _coincenterInfo, commonAPI),
      _marketsCache(CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kMarkets), _cachedResultVault),
                    _curlHandle, exchangeConfig()),
      _allOrderBooksCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kAllOrderBooks), _cachedResultVault),
          _marketsCache, _curlHandle, exchangeConfig()),
      _orderbookCache(CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kOrderBook), _cachedResultVault),
                      _curlHandle, exchangeConfig()),
      _tradedVolumeCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kTradedVolume), _cachedResultVault),
          _curlHandle),
      _tickerCache(CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kLastPrice), _cachedResultVault),
                   _curlHandle) {}

bool KucoinPublic::healthCheck() {
  json::container result =
      json::container::parse(_curlHandle.query("/api/v1/status", CurlOptions(HttpRequestType::kGet)));
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
  json::container result = PublicQuery(_curlHandle, "/api/v1/currencies");
  vector<CurrencyInfo> currencyInfos;
  currencyInfos.reserve(static_cast<uint32_t>(result.size()));
  for (const json::container& curDetail : result) {
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    CurrencyExchange currencyExchange(
        cur, curStr, curStr,
        curDetail["isDepositEnabled"].get<bool>() ? CurrencyExchange::Deposit::kAvailable
                                                  : CurrencyExchange::Deposit::kUnavailable,
        curDetail["isWithdrawEnabled"].get<bool>() ? CurrencyExchange::Withdraw::kAvailable
                                                   : CurrencyExchange::Withdraw::kUnavailable,
        _commonApi.queryIsCurrencyCodeFiat(cur) ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto);

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
  json::container result = PublicQuery(_curlHandle, "/api/v1/symbols");

  MarketSet markets;
  MarketInfoMap marketInfoMap;

  markets.reserve(static_cast<MarketSet::size_type>(result.size()));

  const CurrencyCodeSet& excludedCurrencies = _exchangeConfig.excludedCurrenciesAll();

  for (const json::container& marketDetails : result) {
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
  log::debug("Retrieved {} markets for kucoin", markets.size());
  return {std::move(markets), std::move(marketInfoMap)};
}

MonetaryAmountByCurrencySet KucoinPublic::queryWithdrawalFees() {
  MonetaryAmountVector fees;
  const auto& tradableCurrencies = _tradableCurrenciesCache.get();
  fees.reserve(tradableCurrencies.size());
  for (const TradableCurrenciesFunc::CurrencyInfo& curDetail : tradableCurrencies) {
    fees.push_back(curDetail.withdrawalMinFee);
    log::trace("Retrieved {} withdrawal fee {}", _name, curDetail.withdrawalMinFee);
  }

  log::info("Retrieved {} withdrawal fees for {} coins", _name, fees.size());
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
  const json::container data = PublicQuery(_curlHandle, "/api/v1/market/allTickers");
  const auto time = Clock::now();
  const auto tickerIt = data.find("ticker");
  if (tickerIt == data.end()) {
    return ret;
  }
  for (const json::container& tickerDetails : *tickerIt) {
    Market mk(tickerDetails["symbol"].get<std::string_view>(), '-');
    if (!markets.contains(mk)) {
      log::debug("Market {} is not present", mk);
      continue;
    }
    MonetaryAmount askPri(tickerDetails["sell"].get<std::string_view>(), mk.quote());
    if (askPri == 0) {
      log::debug("Discarding {} because of invalid ask price {}", mk, askPri);
      continue;
    }
    MonetaryAmount bidPri(tickerDetails["buy"].get<std::string_view>(), mk.quote());
    if (bidPri == 0) {
      log::debug("Discarding {} because of invalid bid price {}", mk, bidPri);
      continue;
    }
    // There is no volume in the response, we need to emulate it, based on the 24h volume
    MonetaryAmount dayVolume(tickerDetails["vol"].get<std::string_view>(), mk.base());
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

  const json::container asksAndBids = PublicQuery(_curlHandle, endpoint, GetSymbolPostData(mk));
  const auto nowTime = Clock::now();
  const auto asksIt = asksAndBids.find("asks");
  const auto bidsIt = asksAndBids.find("bids");
  if (asksIt != asksAndBids.end() && bidsIt != asksAndBids.end()) {
    orderBookLines.reserve(std::min(static_cast<decltype(depth)>(asksIt->size()), depth) +
                           std::min(static_cast<decltype(depth)>(bidsIt->size()), depth));

    // Reverse iterate as bids are received in descending order
    for (const auto& val : *bidsIt | std::views::reverse | std::ranges::views::take(depth)) {
      MonetaryAmount price(val[0].get<std::string_view>(), mk.quote());
      MonetaryAmount amount(val[1].get<std::string_view>(), mk.base());

      orderBookLines.pushBid(amount, price);
    }

    for (const auto& val : *asksIt | std::ranges::views::take(depth)) {
      MonetaryAmount price(val[0].get<std::string_view>(), mk.quote());
      MonetaryAmount amount(val[1].get<std::string_view>(), mk.base());

      orderBookLines.pushAsk(amount, price);
    }
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
  const json::container result = PublicQuery(_curlHandle, "/api/v1/market/stats", GetSymbolPostData(mk));
  const auto volIt = result.find("vol");
  const std::string_view amountStr = volIt == result.end() ? std::string_view() : volIt->get<std::string_view>();
  return {amountStr, mk.base()};
}

PublicTradeVector KucoinPublic::queryLastTrades(Market mk, int nbTrades) {
  static constexpr auto kMaxNbLastTrades = 100;
  if (nbTrades > kMaxNbLastTrades) {
    log::warn("Maximum number of last trades to query from {} is {}", name(), kMaxNbLastTrades);
  }

  json::container result = PublicQuery(_curlHandle, "/api/v1/market/histories", GetSymbolPostData(mk));

  PublicTradeVector ret;
  ret.reserve(std::min(static_cast<PublicTradeVector::size_type>(result.size()),
                       static_cast<PublicTradeVector::size_type>(nbTrades)));

  for (const json::container& detail : result | std::ranges::views::take(nbTrades)) {
    const MonetaryAmount amount(detail["size"].get<std::string_view>(), mk.base());
    const MonetaryAmount price(detail["price"].get<std::string_view>(), mk.quote());
    // time is in nanoseconds
    const int64_t millisecondsSinceEpoch = static_cast<int64_t>(detail["time"].get<uintmax_t>() / 1000000UL);
    const TradeSide tradeSide = detail["side"].get<std::string_view>() == "buy" ? TradeSide::kBuy : TradeSide::kSell;

    ret.emplace_back(tradeSide, amount, price, TimePoint(milliseconds(millisecondsSinceEpoch)));
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount KucoinPublic::TickerFunc::operator()(Market mk) {
  const json::container result = PublicQuery(_curlHandle, "/api/v1/market/orderbook/level1", GetSymbolPostData(mk));
  const auto priceIt = result.find("price");
  const std::string_view amountStr = priceIt == result.end() ? std::string_view() : priceIt->get<std::string_view>();
  return {amountStr, mk.quote()};
}
}  // namespace cct::api
