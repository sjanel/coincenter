#include "upbitpublicapi.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "cachedresult.hpp"
#include "cct_json-container.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangeconfig.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "file.hpp"
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
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct::api {
namespace {

json::container PublicQuery(CurlHandle& curlHandle, std::string_view endpoint,
                            CurlPostData&& postData = CurlPostData()) {
  RequestRetry requestRetry(curlHandle, CurlOptions(HttpRequestType::kGet, std::move(postData)));

  return requestRetry.queryJson(endpoint, [](const json::container& jsonResponse) {
    const auto foundErrorIt = jsonResponse.find("error");
    if (foundErrorIt != jsonResponse.end()) {
      const auto statusCodeIt = jsonResponse.find("name");
      const long statusCode = statusCodeIt == jsonResponse.end() ? -1 : statusCodeIt->get<long>();
      const auto msgIt = jsonResponse.find("message");
      const std::string_view msg = msgIt == jsonResponse.end() ? "Unknown" : msgIt->get<std::string_view>();
      log::warn("Upbit error ({}, '{}'), full: '{}'", statusCode, msg, jsonResponse.dump());
      return RequestRetry::Status::kResponseError;
    }
    return RequestRetry::Status::kResponseOK;
  });
}

}  // namespace

UpbitPublic::UpbitPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CommonAPI& commonAPI)
    : ExchangePublic("upbit", fiatConverter, commonAPI, config),
      _curlHandle(kUrlBase, config.metricGatewayPtr(),
                  exchangeConfig().curlOptionsBuilderBase(ExchangeConfig::Api::kPublic).build(), config.getRunMode()),
      _marketsCache(CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kMarkets), _cachedResultVault),
                    _curlHandle, exchangeConfig()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), _curlHandle,
          _marketsCache),
      _withdrawalFeesCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault), _name,
          config.dataDir()),
      _allOrderBooksCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kAllOrderBooks), _cachedResultVault),
          _curlHandle, exchangeConfig(), _marketsCache),
      _orderbookCache(CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kOrderBook), _cachedResultVault),
                      _curlHandle, exchangeConfig()),
      _tradedVolumeCache(
          CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kTradedVolume), _cachedResultVault),
          _curlHandle),
      _tickerCache(CachedResultOptions(exchangeConfig().getAPICallUpdateFrequency(kLastPrice), _cachedResultVault),
                   _curlHandle) {}

bool UpbitPublic::healthCheck() {
  static constexpr auto kAllowExceptions = false;
  json::container result = json::container::parse(
      _curlHandle.query("/v1/ticker", CurlOptions(HttpRequestType::kGet, {{"markets", "KRW-BTC"}})), nullptr,
      kAllowExceptions);
  if (result.is_discarded()) {
    log::error("{} health check response badly formatted", _name);
    return false;
  }
  auto errorIt = result.find("error");
  if (errorIt != result.end()) {
    log::error("Error in {} status: {}", _name, errorIt->dump());
    return false;
  }
  return !result.empty() && result.is_array() && result.front().find("timestamp") != result.front().end();
}

std::optional<MonetaryAmount> UpbitPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  const auto& map = _withdrawalFeesCache.get();
  auto it = map.find(currencyCode);
  if (it == map.end()) {
    return {};
  }
  return *it;
}

CurrencyExchangeFlatSet UpbitPublic::TradableCurrenciesFunc::operator()() {
  const MarketSet& markets = _marketsCache.get();
  CurrencyExchangeFlatSet currencies;
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
  json::container result = PublicQuery(_curlHandle, "/v1/market/all", {{"isDetails", "true"}});
  const CurrencyCodeSet& excludedCurrencies = _exchangeConfig.excludedCurrenciesAll();
  MarketSet ret;
  ret.reserve(static_cast<MarketSet::size_type>(result.size()));
  for (const json::container& marketDetails : result) {
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

MonetaryAmountByCurrencySet UpbitPublic::WithdrawalFeesFunc::operator()() {
  MonetaryAmountVector fees;
  File withdrawFeesFile(_dataDir, File::Type::kStatic, "withdrawfees.json", File::IfError::kThrow);
  json::container jsonData = withdrawFeesFile.readAllJson();
  for (const auto& [coin, value] : jsonData[_name].items()) {
    CurrencyCode coinAcro(coin);
    MonetaryAmount ma(value.get<std::string_view>(), coinAcro);
    log::debug("Updated Upbit withdrawal fees {}", ma);
    fees.push_back(ma);
  }
  log::info("Updated Upbit withdrawal fees for {} coins", fees.size());
  return MonetaryAmountByCurrencySet(std::move(fees));
}

namespace {

template <class OutputType>
OutputType ParseOrderBooks(const json::container& result, int depth) {
  OutputType ret;
  const auto time = Clock::now();

  MarketOrderBookLines orderBookLines;

  for (const json::container& marketDetails : result) {
    std::string_view marketStr = marketDetails["market"].get<std::string_view>();
    std::size_t dashPos = marketStr.find('-');
    if (dashPos == std::string_view::npos) {
      log::error("Unable to parse order book json for market {}", marketStr);
      continue;
    }

    /// Remember, Upbit markets are inverted, quote first then base
    CurrencyCode quote(marketStr.substr(0, dashPos));
    CurrencyCode base(marketStr.substr(dashPos + 1));
    Market market(base, quote);

    const auto& orderBookLinesJson = marketDetails["orderbook_units"];

    orderBookLines.clear();
    orderBookLines.reserve(orderBookLinesJson.size() * 2U);

    for (const json::container& orderbookDetails : orderBookLinesJson | std::ranges::views::take(depth)) {
      // Amounts are not strings, but doubles
      MonetaryAmount askPri(orderbookDetails["ask_price"].get<double>(), quote);
      MonetaryAmount bidPri(orderbookDetails["bid_price"].get<double>(), quote);
      MonetaryAmount askVol(orderbookDetails["ask_size"].get<double>(), base);
      MonetaryAmount bidVol(orderbookDetails["bid_size"].get<double>(), base);

      orderBookLines.pushAsk(askVol, askPri);
      orderBookLines.pushBid(bidVol, bidPri);
    }
    if (static_cast<int>(orderBookLines.size() / 2) < depth) {
      log::warn("Upbit does not support orderbook depth larger than {}", orderBookLines.size() / 2);
    }
    if constexpr (std::is_same_v<OutputType, MarketOrderBookMap>) {
      ret.insert_or_assign(market, MarketOrderBook(time, market, orderBookLines));
    } else {
      ret = MarketOrderBook(time, market, orderBookLines);
    }
  }
  if constexpr (std::is_same_v<OutputType, MarketOrderBookMap>) {
    if (ret.size() > 1) {
      log::info("Retrieved {} order books from Upbit", ret.size());
    }
  }

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
  return ParseOrderBooks<MarketOrderBookMap>(PublicQuery(_curlHandle, "/v1/orderbook", {{"markets", marketsStr}}),
                                             depth);
}

MarketOrderBook UpbitPublic::OrderBookFunc::operator()(Market mk, int depth) {
  return ParseOrderBooks<MarketOrderBook>(
      PublicQuery(_curlHandle, "/v1/orderbook", {{"markets", ReverseMarketStr(mk)}}), depth);
}

MonetaryAmount UpbitPublic::TradedVolumeFunc::operator()(Market mk) {
  json::container result =
      PublicQuery(_curlHandle, "/v1/candles/days", {{"count", 1}, {"market", ReverseMarketStr(mk)}});
  double last24hVol = result.empty() ? 0 : result.front()["candle_acc_trade_volume"].get<double>();
  return MonetaryAmount(last24hVol, mk.base());
}

PublicTradeVector UpbitPublic::queryLastTrades(Market mk, int nbTrades) {
  json::container result =
      PublicQuery(_curlHandle, "/v1/trades/ticks", {{"count", nbTrades}, {"market", ReverseMarketStr(mk)}});

  PublicTradeVector ret;
  ret.reserve(static_cast<PublicTradeVector::size_type>(result.size()));

  for (const json::container& detail : result) {
    MonetaryAmount amount(detail["trade_volume"].get<double>(), mk.base());
    MonetaryAmount price(detail["trade_price"].get<double>(), mk.quote());
    int64_t millisecondsSinceEpoch = detail["timestamp"].get<int64_t>();
    TradeSide tradeSide = detail["ask_bid"].get<std::string_view>() == "BID" ? TradeSide::kBuy : TradeSide::kSell;

    ret.emplace_back(tradeSide, amount, price, TimePoint(milliseconds(millisecondsSinceEpoch)));
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount UpbitPublic::TickerFunc::operator()(Market mk) {
  json::container result =
      PublicQuery(_curlHandle, "/v1/trades/ticks", {{"count", 1}, {"market", ReverseMarketStr(mk)}});
  double lastPrice = result.empty() ? 0 : result.front()["trade_price"].get<double>();
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
    static constexpr std::array kMinOrderAmounts = {MonetaryAmount(5000, "KRW"),
                                                    MonetaryAmount(5, "BTC", 4)};  // 5000 KRW or 0.0005 BTC is min
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
