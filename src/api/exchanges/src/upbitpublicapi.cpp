#include "upbitpublicapi.hpp"

#include <algorithm>
#include <cassert>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "fiatconverter.hpp"
#include "jsonhelpers.hpp"

namespace cct {
namespace api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, CurlPostData&& postData = CurlPostData()) {
  std::string method_url = UpbitPublic::kUrlBase;
  method_url.append("/v1/");
  method_url.append(endpoint);

  CurlOptions opts(CurlOptions::RequestType::kGet, std::move(postData));
  opts.userAgent = UpbitPublic::kUserAgent;

  json dataJson = json::parse(curlHandle.query(method_url, opts));
  //{"error":{"name":400,"message":"Type mismatch error. Check the parameters type!"}}
  if (dataJson.contains("error")) {
    const long statusCode = dataJson["name"].get<long>();
    std::string_view msg = dataJson["message"].get<std::string_view>();
    throw exception("error: " + std::to_string(statusCode) + " \"" + std::string(msg) + "\"");
  }
  return dataJson;
}

bool CheckCurrencyCode(CurrencyCode standardCode, const ExchangeInfo::CurrencySet& excludedCurrencies) {
  if (excludedCurrencies.contains(standardCode)) {
    // Forbidden currency, do not consider its market
    log::trace("Discard {} excluded by config", standardCode.str());
    return false;
  }
  return true;
}

}  // namespace

UpbitPublic::UpbitPublic(CoincenterInfo& config, FiatConverter& fiatConverter, CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("upbit", fiatConverter, cryptowatchAPI),
      _curlHandle(config.exchangeInfo(_name).minPublicQueryDelay(), config.getRunMode()),
      _marketsCache(CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kMarkets), _cachedResultVault),
                    _curlHandle, config.exchangeInfo(_name)),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault),
          _curlHandle, _marketsCache),
      _withdrawalFeesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kWithdrawalFees), _cachedResultVault),
          _name),
      _allOrderBooksCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAllOrderBooks), _cachedResultVault),
          config, _curlHandle, config.exchangeInfo(_name), _marketsCache),
      _orderbookCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kOrderBook), _cachedResultVault), config,
          _curlHandle, config.exchangeInfo(_name)) {}

CurrencyExchangeFlatSet UpbitPublic::TradableCurrenciesFunc::operator()() {
  const MarketSet& markets = _marketsCache.get();
  CurrencyExchangeFlatSet currencies;
  currencies.reserve(markets.size() / 2);
  for (Market m : markets) {
    currencies.insert(CurrencyExchange(m.base(), m.base(), m.base()));
    currencies.insert(CurrencyExchange(m.quote(), m.quote(), m.quote()));
  }
  log::info("Retrieved {} Upbit currencies with partial information", currencies.size());
  log::warn("Use Upbit private API to get full withdrawal and deposit statuses");
  return currencies;
}

ExchangePublic::MarketSet UpbitPublic::MarketsFunc::operator()() {
  json result = PublicQuery(_curlHandle, "market/all", {{"isDetails", "true"}});
  const ExchangeInfo::CurrencySet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  MarketSet ret;
  ret.reserve(result.size());
  for (const json& marketDetails : result) {
    //{"market_warning":"NONE","market":"KRW-BTC","korean_name":"비트코인","english_name":"Bitcoin"}
    std::string_view marketStr = marketDetails["market"].get<std::string_view>();
    std::string_view marketWarningStr = marketDetails["market_warning"].get<std::string_view>();
    if (marketWarningStr != "NONE") {
      log::debug("Discard Upbit market {} as it has a warning {}", marketStr, marketWarningStr);
      continue;
    }
    // Upbit markets are inverted
    std::size_t dashPos = marketStr.find_first_of('-');
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
  json jsonData = OpenJsonFile("withdrawfees", FileNotFoundMode::kThrow);
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
ExchangePublic::MarketOrderBookMap ParseOrderBooks(const json& result) {
  ExchangePublic::MarketOrderBookMap ret;
  for (const json& marketDetails : result) {
    std::string_view marketStr = marketDetails["market"].get<std::string_view>();
    std::size_t dashPos = marketStr.find_first_of('-');
    if (dashPos == std::string_view::npos) {
      log::error("Unable to parse order book json for market {}", marketStr);
      continue;
    }

    cct::SmallVector<OrderBookLine, 10> orderBookLines;

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
    }
    ret.insert_or_assign(market, MarketOrderBook(market, orderBookLines));
  }
  log::info("Retrieved {} orderbooks from Upbit", ret.size());
  return ret;
}
}  // namespace

ExchangePublic::MarketOrderBookMap UpbitPublic::AllOrderBooksFunc::operator()(int) {
  const MarketSet& markets = _marketsCache.get();
  std::string marketsStr;
  marketsStr.reserve(markets.size() * 8);
  for (Market m : markets) {
    if (!marketsStr.empty()) {
      marketsStr.push_back(',');
    }
    marketsStr.append(m.reverse().assetsPairStr('-'));
  }
  return ParseOrderBooks(PublicQuery(_curlHandle, "orderbook", {{"markets", marketsStr}}));
}

MarketOrderBook UpbitPublic::OrderBookFunc::operator()(Market m, int) {
  ExchangePublic::MarketOrderBookMap marketOrderBookMap =
      ParseOrderBooks(PublicQuery(_curlHandle, "orderbook", {{"markets", m.reverse().assetsPairStr('-')}}));
  auto it = marketOrderBookMap.find(m);
  if (it == marketOrderBookMap.end()) {
    throw exception("Unexpected answer from get OrderBooks");
  }
  return it->second;
}

}  // namespace api
}  // namespace cct
