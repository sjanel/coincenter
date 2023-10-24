#include "upbitpublicapi.hpp"

#include <algorithm>
#include <cassert>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "fiatconverter.hpp"
#include "file.hpp"
#include "stringhelpers.hpp"

namespace cct::api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, CurlPostData&& postData = CurlPostData()) {
  json ret = json::parse(curlHandle.query(endpoint, CurlOptions(HttpRequestType::kGet, std::move(postData))));
  //{"error":{"name":400,"message":"Type mismatch error. Check the parameters type!"}}
  auto errorIt = ret.find("error");
  if (errorIt != ret.end()) {
    log::error("Full Upbit json error: '{}'", ret.dump());
    auto statusCodeIt = ret.find("name");
    const long statusCode = statusCodeIt == ret.end() ? -1 : statusCodeIt->get<long>();
    auto msgIt = ret.find("message");
    std::string_view msg = msgIt == ret.end() ? "Unknown" : msgIt->get<std::string_view>();
    throw exception("Upbit error: {}, code = {}", msg, statusCode);
  }
  return ret;
}

}  // namespace

UpbitPublic::UpbitPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CommonAPI& commonAPI)
    : ExchangePublic("upbit", fiatConverter, commonAPI, config),
      _curlHandle(kUrlBase, config.metricGatewayPtr(),
                  PermanentCurlOptions::Builder()
                      .setMinDurationBetweenQueries(exchangeInfo().publicAPIRate())
                      .setAcceptedEncoding(exchangeInfo().acceptEncoding())
                      .setRequestCallLogLevel(exchangeInfo().requestsCallLogLevel())
                      .setRequestAnswerLogLevel(exchangeInfo().requestsAnswerLogLevel())
                      .build(),
                  config.getRunMode()),
      _marketsCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kMarkets), _cachedResultVault),
                    _curlHandle, exchangeInfo()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), _curlHandle,
          _marketsCache),
      _withdrawalFeesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault), _name,
          config.dataDir()),
      _allOrderBooksCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kAllOrderBooks), _cachedResultVault),
          _curlHandle, exchangeInfo(), _marketsCache),
      _orderbookCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kOrderBook), _cachedResultVault),
                      _curlHandle, exchangeInfo()),
      _tradedVolumeCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kTradedVolume), _cachedResultVault),
          _curlHandle),
      _tickerCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kLastPrice), _cachedResultVault),
                   _curlHandle) {}

bool UpbitPublic::healthCheck() {
  json result =
      json::parse(_curlHandle.query("/v1/ticker", CurlOptions(HttpRequestType::kGet, {{"markets", "KRW-BTC"}})));
  auto errorIt = result.find("error");
  if (errorIt != result.end()) {
    log::error("Error in {} status: {}", _name, errorIt->dump());
    return false;
  }
  return !result.empty() && result.is_array() && result.front().find("timestamp") != result.front().end();
}

MonetaryAmount UpbitPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  const auto& map = _withdrawalFeesCache.get();
  auto it = map.find(currencyCode);
  if (it == map.end()) {
    throw exception("Unable to find currency code in withdrawal fees");
  }
  return it->second;
}

CurrencyExchangeFlatSet UpbitPublic::TradableCurrenciesFunc::operator()() {
  const MarketSet& markets = _marketsCache.get();
  CurrencyExchangeFlatSet currencies;
  currencies.reserve(markets.size() / 2);
  for (Market mk : markets) {
    currencies.emplace(mk.base(), mk.base(), mk.base());
    currencies.emplace(mk.quote(), mk.quote(), mk.quote());
  }
  log::warn("Retrieved {} Upbit currencies with partial information", currencies.size());
  log::warn("Public API of Upbit does not provide deposit / withdrawal access");
  log::warn("Use Upbit private API to get full withdrawal and deposit statuses");
  return currencies;
}

bool UpbitPublic::CheckCurrencyCode(CurrencyCode standardCode, const CurrencyCodeSet& excludedCurrencies) {
  if (excludedCurrencies.contains(standardCode)) {
    // Forbidden currency, do not consider its market
    log::trace("Discard {} excluded by config", standardCode);
    return false;
  }
  return true;
}

MarketSet UpbitPublic::MarketsFunc::operator()() {
  json result = PublicQuery(_curlHandle, "/v1/market/all", {{"isDetails", "true"}});
  const CurrencyCodeSet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  MarketSet ret;
  ret.reserve(static_cast<MarketSet::size_type>(result.size()));
  for (const json& marketDetails : result) {
    std::string_view marketStr = marketDetails["market"].get<std::string_view>();
    std::string_view marketWarningStr = marketDetails["market_warning"].get<std::string_view>();
    if (marketWarningStr != "NONE") {
      log::debug("Discard Upbit market {} as it has warning {}", marketStr, marketWarningStr);
      continue;
    }
    // Upbit markets are inverted
    std::size_t dashPos = marketStr.find('-');
    if (dashPos == std::string_view::npos) {
      log::error("Discard Upbit market {} as unable to parse the currency codes in it", marketStr);
      continue;
    }
    CurrencyCode quote(std::string_view(marketStr.begin(), marketStr.begin() + dashPos));
    if (!CheckCurrencyCode(quote, excludedCurrencies)) {
      continue;
    }
    CurrencyCode base(std::string_view(marketStr.begin() + dashPos + 1, marketStr.end()));
    if (!CheckCurrencyCode(base, excludedCurrencies)) {
      continue;
    }
    auto mkIt = ret.emplace(base, quote).first;
    log::debug("Retrieved Upbit market {}", *mkIt);
  }
  log::info("Retrieved {} markets from Upbit", ret.size());
  return ret;
}

WithdrawalFeeMap UpbitPublic::WithdrawalFeesFunc::operator()() {
  WithdrawalFeeMap ret;
  File withdrawFeesFile(_dataDir, File::Type::kStatic, "withdrawfees.json", File::IfError::kThrow);
  json jsonData = withdrawFeesFile.readAllJson();
  for (const auto& [coin, value] : jsonData[_name].items()) {
    CurrencyCode coinAcro(coin);
    MonetaryAmount ma(value.get<std::string_view>(), coinAcro);
    log::debug("Updated Upbit withdrawal fees {}", ma);
    ret.insert_or_assign(coinAcro, ma);
  }
  log::info("Updated Upbit withdrawal fees for {} coins", ret.size());
  return ret;
}

namespace {
MarketOrderBookMap ParseOrderBooks(const json& result, int depth) {
  MarketOrderBookMap ret;
  for (const json& marketDetails : result) {
    std::string_view marketStr = marketDetails["market"].get<std::string_view>();
    std::size_t dashPos = marketStr.find('-');
    if (dashPos == std::string_view::npos) {
      log::error("Unable to parse order book json for market {}", marketStr);
      continue;
    }

    SmallVector<OrderBookLine, 10> orderBookLines;

    /// Remember, Upbit markets are inverted, quote first then base
    CurrencyCode quote(std::string_view(marketStr.begin(), marketStr.begin() + dashPos));
    CurrencyCode base(std::string_view(marketStr.begin() + dashPos + 1, marketStr.end()));
    Market market(base, quote);
    for (const json& orderbookDetails : marketDetails["orderbook_units"]) {
      // Amounts are not strings, but doubles
      MonetaryAmount askPri(orderbookDetails["ask_price"].get<double>(), quote);
      MonetaryAmount bidPri(orderbookDetails["bid_price"].get<double>(), quote);
      MonetaryAmount askVol(orderbookDetails["ask_size"].get<double>(), base);
      MonetaryAmount bidVol(orderbookDetails["bid_size"].get<double>(), base);

      orderBookLines.emplace_back(askVol, askPri, true /* isAsk */);
      orderBookLines.emplace_back(bidVol, bidPri, false /* isAsk */);

      if (static_cast<int>(orderBookLines.size() / 2) == depth) {
        // Upbit does not have a depth parameter, the only thing we can do is to truncate it manually
        break;
      }
    }
    if (static_cast<int>(orderBookLines.size() / 2) < depth) {
      log::warn("Upbit does not support orderbook depth larger than {}", orderBookLines.size() / 2);
    }
    ret.insert_or_assign(market, MarketOrderBook(market, orderBookLines));
  }
  log::info("Retrieved {} orderbooks from Upbit", ret.size());
  return ret;
}
}  // namespace

MarketOrderBookMap UpbitPublic::AllOrderBooksFunc::operator()(int depth) {
  const MarketSet& markets = _marketsCache.get();
  string marketsStr;
  marketsStr.reserve(static_cast<string::size_type>(markets.size()) * 8);
  for (Market mk : markets) {
    if (!marketsStr.empty()) {
      marketsStr.push_back(',');
    }
    marketsStr.append(ReverseMarketStr(mk));
  }
  return ParseOrderBooks(PublicQuery(_curlHandle, "/v1/orderbook", {{"markets", marketsStr}}), depth);
}

MarketOrderBook UpbitPublic::OrderBookFunc::operator()(Market mk, int depth) {
  MarketOrderBookMap marketOrderBookMap =
      ParseOrderBooks(PublicQuery(_curlHandle, "/v1/orderbook", {{"markets", ReverseMarketStr(mk)}}), depth);
  auto it = marketOrderBookMap.find(mk);
  if (it == marketOrderBookMap.end()) {
    throw exception("Unexpected answer from get OrderBooks");
  }
  return it->second;
}

MonetaryAmount UpbitPublic::TradedVolumeFunc::operator()(Market mk) {
  json result = PublicQuery(_curlHandle, "/v1/candles/days", {{"count", 1}, {"market", ReverseMarketStr(mk)}});
  double last24hVol = result.front()["candle_acc_trade_volume"].get<double>();
  return MonetaryAmount(last24hVol, mk.base());
}

LastTradesVector UpbitPublic::queryLastTrades(Market mk, int nbTrades) {
  json result = PublicQuery(_curlHandle, "/v1/trades/ticks", {{"count", nbTrades}, {"market", ReverseMarketStr(mk)}});
  LastTradesVector ret;
  ret.reserve(static_cast<LastTradesVector::size_type>(result.size()));
  for (const json& detail : result) {
    MonetaryAmount amount(detail["trade_volume"].get<double>(), mk.base());
    MonetaryAmount price(detail["trade_price"].get<double>(), mk.quote());
    int64_t millisecondsSinceEpoch = detail["timestamp"].get<int64_t>();
    TradeSide tradeSide = detail["ask_bid"].get<std::string_view>() == "BID" ? TradeSide::kBuy : TradeSide::kSell;

    ret.emplace_back(tradeSide, amount, price, TimePoint(std::chrono::milliseconds(millisecondsSinceEpoch)));
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount UpbitPublic::TickerFunc::operator()(Market mk) {
  json result = PublicQuery(_curlHandle, "/v1/trades/ticks", {{"count", 1}, {"market", ReverseMarketStr(mk)}});
  double lastPrice = result.front()["trade_price"].get<double>();
  return MonetaryAmount(lastPrice, mk.quote());
}

MonetaryAmount UpbitPublic::SanitizeVolume(MonetaryAmount vol, MonetaryAmount pri) {
  // Upbit can return this error for big trades:
  // "최대매수금액 1000000000.0 KRW 보다 작은 주문을 입력해 주세요."
  // It means that total value of the order should not exceed 1000000000 KRW.
  // Let's adjust volume to avoid this issue.
  static constexpr MonetaryAmount kMaximumOrderValue = MonetaryAmount(1000000000, CurrencyCode("KRW"));
  MonetaryAmount ret = vol;
  if (pri.currencyCode() == kMaximumOrderValue.currencyCode() && vol.toNeutral() * pri > kMaximumOrderValue) {
    log::debug("{} / {} = {}", kMaximumOrderValue, pri, kMaximumOrderValue / pri);
    ret = MonetaryAmount(kMaximumOrderValue / pri, vol.currencyCode());
    log::debug("Order too big, decrease volume {} to {}", vol, ret);
  } else {
    /// Value found in this page:
    /// https://cryptoexchangenews.net/2021/02/upbit-notes-information-on-changing-the-minimum-order-amount-at-krw-market-to-stabilize-the/
    /// confirmed with some tests. However, it could change in the future.
    static constexpr std::array<MonetaryAmount, 2> kMinOrderAmounts{
        {MonetaryAmount(5000, "KRW"), MonetaryAmount(5, "BTC", 4)}};  // 5000 KRW or 0.0005 BTC is min
    for (MonetaryAmount minOrderAmount : kMinOrderAmounts) {
      if (vol.currencyCode() == minOrderAmount.currencyCode()) {
        if (vol < minOrderAmount) {
          ret = minOrderAmount;
          break;
        }
      } else if (pri.currencyCode() == minOrderAmount.currencyCode()) {
        MonetaryAmount orderAmount(vol.toNeutral() * pri);
        // vol * pri = minOrderAmount, vol = minOrderAmount / pri
        if (orderAmount < minOrderAmount) {
          ret = MonetaryAmount(minOrderAmount / pri, vol.currencyCode());
          break;
        }
      }
    }
  }
  if (ret != vol) {
    log::warn("Sanitize volume {} -> {}", vol, ret);
  }
  return ret;
}

}  // namespace cct::api
