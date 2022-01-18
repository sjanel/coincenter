#include "upbitpublicapi.hpp"

#include <algorithm>
#include <cassert>

#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "fiatconverter.hpp"

namespace cct {
namespace api {
namespace {

string GetMethodUrl(std::string_view endpoint) {
  string methodUrl(UpbitPublic::kUrlBase);
  methodUrl.append("/v1/");
  methodUrl.append(endpoint);
  return methodUrl;
}

json PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, CurlPostData&& postData = CurlPostData()) {
  json dataJson = json::parse(curlHandle.query(
      GetMethodUrl(endpoint), CurlOptions(HttpRequestType::kGet, std::move(postData), UpbitPublic::kUserAgent)));
  //{"error":{"name":400,"message":"Type mismatch error. Check the parameters type!"}}
  if (dataJson.contains("error")) {
    const long statusCode = dataJson["name"].get<long>();
    std::string_view msg = dataJson["message"].get<std::string_view>();
    throw exception("error: " + MonetaryAmount(statusCode).amountStr() + " \"" + string(msg) + "\"");
  }
  return dataJson;
}

}  // namespace

UpbitPublic::UpbitPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("upbit", fiatConverter, cryptowatchAPI, config),
      _curlHandle(config.metricGatewayPtr(), config.exchangeInfo(_name).minPublicQueryDelay(), config.getRunMode()),
      _marketsCache(CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kMarkets), _cachedResultVault),
                    _curlHandle, config.exchangeInfo(_name)),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault),
          _curlHandle, _marketsCache),
      _withdrawalFeesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kWithdrawalFees), _cachedResultVault),
          _name, config.dataDir()),
      _allOrderBooksCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAllOrderBooks), _cachedResultVault),
          _curlHandle, config.exchangeInfo(_name), _marketsCache),
      _orderbookCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kOrderBook), _cachedResultVault),
          _curlHandle, config.exchangeInfo(_name)),
      _tradedVolumeCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kTradedVolume), _cachedResultVault),
          _curlHandle),
      _tickerCache(CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kLastPrice), _cachedResultVault),
                   _curlHandle) {}

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
  for (Market m : markets) {
    currencies.emplace(m.base(), m.base(), m.base());
    currencies.emplace(m.quote(), m.quote(), m.quote());
  }
  log::warn("Retrieved {} Upbit currencies with partial information", currencies.size());
  log::warn("Public API of Upbit does not provide deposit / withdrawal access");
  log::warn("Use Upbit private API to get full withdrawal and deposit statuses");
  return currencies;
}

bool UpbitPublic::CheckCurrencyCode(CurrencyCode standardCode, const ExchangeInfo::CurrencySet& excludedCurrencies) {
  if (excludedCurrencies.contains(standardCode)) {
    // Forbidden currency, do not consider its market
    log::trace("Discard {} excluded by config", standardCode.str());
    return false;
  }
  return true;
}

ExchangePublic::MarketSet UpbitPublic::MarketsFunc::operator()() {
  json result = PublicQuery(_curlHandle, "market/all", {{"isDetails", "true"}});
  const ExchangeInfo::CurrencySet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  MarketSet ret;
  ret.reserve(static_cast<MarketSet::size_type>(result.size()));
  for (const json& marketDetails : result) {
    std::string_view marketStr = marketDetails["market"].get<std::string_view>();
    std::string_view marketWarningStr = marketDetails["market_warning"].get<std::string_view>();
    if (marketWarningStr != "NONE") {
      log::debug("Discard Upbit market {} as it has a warning {}", marketStr, marketWarningStr);
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
    log::debug("Retrieved Upbit market {}", mkIt->str());
  }
  log::info("Retrieved {} markets from Upbit", ret.size());
  return ret;
}

ExchangePublic::WithdrawalFeeMap UpbitPublic::WithdrawalFeesFunc::operator()() {
  WithdrawalFeeMap ret;
  File withdrawFeesFile(_dataDir, File::Type::kStatic, "withdrawfees.json", File::IfNotFound::kThrow);
  json jsonData = withdrawFeesFile.readJson();
  for (const auto& [coin, value] : jsonData[_name].items()) {
    CurrencyCode coinAcro(coin);
    MonetaryAmount ma(value.get<std::string_view>(), coinAcro);
    log::debug("Updated Upbit withdrawal fees {}", ma.str());
    ret.insert_or_assign(coinAcro, ma);
  }
  log::info("Updated Upbit withdrawal fees for {} coins", ret.size());
  return ret;
}

namespace {
ExchangePublic::MarketOrderBookMap ParseOrderBooks(const json& result, int depth) {
  ExchangePublic::MarketOrderBookMap ret;
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

ExchangePublic::MarketOrderBookMap UpbitPublic::AllOrderBooksFunc::operator()(int depth) {
  const MarketSet& markets = _marketsCache.get();
  string marketsStr;
  marketsStr.reserve(static_cast<string::size_type>(markets.size()) * 8);
  for (Market m : markets) {
    if (!marketsStr.empty()) {
      marketsStr.push_back(',');
    }
    marketsStr.append(m.reverse().assetsPairStr('-'));
  }
  return ParseOrderBooks(PublicQuery(_curlHandle, "orderbook", {{"markets", marketsStr}}), depth);
}

MarketOrderBook UpbitPublic::OrderBookFunc::operator()(Market m, int depth) {
  ExchangePublic::MarketOrderBookMap marketOrderBookMap =
      ParseOrderBooks(PublicQuery(_curlHandle, "orderbook", {{"markets", m.reverse().assetsPairStr('-')}}), depth);
  auto it = marketOrderBookMap.find(m);
  if (it == marketOrderBookMap.end()) {
    throw exception("Unexpected answer from get OrderBooks");
  }
  return it->second;
}

MonetaryAmount UpbitPublic::TradedVolumeFunc::operator()(Market m) {
  json result = PublicQuery(_curlHandle, "candles/days", {{"count", 1}, {"market", m.reverse().assetsPairStr('-')}});
  double last24hVol = result.front()["candle_acc_trade_volume"].get<double>();
  return MonetaryAmount(last24hVol, m.base());
}

UpbitPublic::LastTradesVector UpbitPublic::queryLastTrades(Market m, int nbTrades) {
  json result =
      PublicQuery(_curlHandle, "trades/ticks", {{"count", nbTrades}, {"market", m.reverse().assetsPairStr('-')}});
  LastTradesVector ret;
  ret.reserve(static_cast<LastTradesVector::size_type>(result.size()));
  for (const json& detail : result) {
    MonetaryAmount amount(detail["trade_volume"].get<double>(), m.base());
    MonetaryAmount price(detail["trade_price"].get<double>(), m.quote());
    int64_t millisecondsSinceEpoch = detail["timestamp"].get<int64_t>();
    TradeSide tradeSide = detail["ask_bid"].get<std::string_view>() == "BID" ? TradeSide::kBuy : TradeSide::kSell;

    ret.emplace_back(tradeSide, amount, price,
                     PublicTrade::TimePoint(std::chrono::milliseconds(millisecondsSinceEpoch)));
  }
  std::ranges::sort(ret);
  return ret;
}

MonetaryAmount UpbitPublic::TickerFunc::operator()(Market m) {
  json result = PublicQuery(_curlHandle, "trades/ticks", {{"count", 1}, {"market", m.reverse().assetsPairStr('-')}});
  double lastPrice = result.front()["trade_price"].get<double>();
  return MonetaryAmount(lastPrice, m.quote());
}

}  // namespace api
}  // namespace cct
