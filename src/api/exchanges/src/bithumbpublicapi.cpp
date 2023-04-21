#include "bithumbpublicapi.hpp"

#include <algorithm>
#include <cassert>
#include <ctime>
#include <sstream>
#include <string>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "curloptions.hpp"
#include "fiatconverter.hpp"
#include "monetaryamount.hpp"
#include "stringhelpers.hpp"
#include "timestring.hpp"

namespace cct::api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, CurrencyCode base,
                 CurrencyCode quote = CurrencyCode(), std::string_view urlOpts = "") {
  string methodUrl(endpoint);
  methodUrl.push_back('/');
  base.appendStrTo(methodUrl);
  if (!quote.isNeutral()) {
    methodUrl.push_back('_');
    quote.appendStrTo(methodUrl);
  }
  if (!urlOpts.empty()) {
    methodUrl.push_back('?');
    methodUrl.append(urlOpts);
  }

  json ret = json::parse(curlHandle.query(methodUrl, CurlOptions(HttpRequestType::kGet, BithumbPublic::kUserAgent)));
  auto errorIt = ret.find("status");
  if (errorIt != ret.end()) {
    std::string_view statusCode = errorIt->get<std::string_view>();  // "5300" for instance
    if (statusCode != BithumbPublic::kStatusOKStr) {                 // "0000" stands for: request OK
      log::error("Full Bithumb json error: '{}'", ret.dump());
      auto msgIt = ret.find("message");
      throw exception("Bithumb error: {}, msg: {}", statusCode,
                      msgIt != ret.end() ? msgIt->get<std::string_view>() : "null");
    }
  }
  return ret["data"];
}

}  // namespace

BithumbPublic::BithumbPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("bithumb", fiatConverter, cryptowatchAPI, config),
      _curlHandle(kUrlBase, config.metricGatewayPtr(), exchangeInfo().publicAPIRate(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), config,
          cryptowatchAPI, _curlHandle),
      _withdrawalFeesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault),
          config.metricGatewayPtr(), exchangeInfo().publicAPIRate(), config.getRunMode()),
      _allOrderBooksCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kAllOrderBooks), _cachedResultVault), config,
          _curlHandle, exchangeInfo()),
      _orderbookCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kOrderBook), _cachedResultVault),
                      config, _curlHandle, exchangeInfo()),
      _tradedVolumeCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kTradedVolume), _cachedResultVault),
          _curlHandle) {}

bool BithumbPublic::healthCheck() {
  json result = json::parse(
      _curlHandle.query("/public/assetsstatus/BTC", CurlOptions(HttpRequestType::kGet, BithumbPublic::kUserAgent)));
  auto statusIt = result.find("status");
  if (statusIt == result.end()) {
    log::error("Unexpected answer from {} status: {}", _name, result.dump());
    return false;
  }
  std::string_view statusStr = statusIt->get<std::string_view>();
  log::info("{} status: {}", _name, statusStr);
  return statusStr == kStatusOKStr;
}

MarketSet BithumbPublic::queryTradableMarkets() {
  auto [pMarketOrderbookMap, lastUpdatedTime] = _allOrderBooksCache.retrieve();
  if (pMarketOrderbookMap == nullptr ||
      lastUpdatedTime + exchangeInfo().getAPICallUpdateFrequency(kMarkets) < Clock::now()) {
    pMarketOrderbookMap = std::addressof(_allOrderBooksCache.get());
  }
  MarketSet markets;
  markets.reserve(static_cast<MarketSet::size_type>(pMarketOrderbookMap->size()));
  std::ranges::transform(*pMarketOrderbookMap, std::inserter(markets, markets.end()),
                         [](const auto& it) { return it.first; });
  return markets;
}

MonetaryAmount BithumbPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  const auto& map = _withdrawalFeesCache.get();
  auto it = map.find(currencyCode);
  if (it == map.end()) {
    throw exception("Unable to find {} in withdrawal fees", currencyCode);
  }
  return it->second;
}

MonetaryAmount BithumbPublic::queryLastPrice(Market mk) {
  // Bithumb does not have a REST API endpoint for last price, let's compute it from the orderbook
  std::optional<MonetaryAmount> avgPrice = queryOrderBook(mk).averagePrice();
  if (!avgPrice) {
    log::error("Empty order book for {} on {} cannot compute average price", mk, _name);
    return MonetaryAmount(0, mk.quote());
  }
  return *avgPrice;
}

WithdrawalFeeMap BithumbPublic::WithdrawalFeesFunc::operator()() {
  WithdrawalFeeMap ret;
  // This is not a published API and only a "standard" html page. We will capture the text information in it.
  // Warning, it's not in json format so we will need manual parsing.
  string dataStr = _curlHandle.query("/customer_support/info_fee", CurlOptions(HttpRequestType::kGet));
  // Now, we have the big string containing the html data. The following should work as long as format is unchanged.
  // here is a line containing our coin with its additional withdrawal fees:
  //
  // <tr data-coin="Bitcoin"><td class="money_type tx_c">Bitcoin(BTC)</td><td id="COIN_C0101"><div
  // class="right"></div></td><td><div class="right out_fee">0.001</div></td></tr><tr data-coin="Ethereum"><td
  // class="money_type tx_c">
  static constexpr std::string_view kCoinSep = "tr data-coin=";
  static constexpr std::string_view kFeeSep = "right out_fee";
  for (std::size_t charPos = dataStr.find(kCoinSep); charPos != string::npos;
       charPos = dataStr.find(kCoinSep, charPos)) {
    charPos = dataStr.find("money_type tx_c", charPos);
    charPos = dataStr.find('(', charPos) + 1;
    std::size_t endP = dataStr.find(')', charPos);
    CurrencyCode coinAcro(std::string_view(dataStr.begin() + charPos, dataStr.begin() + endP));
    std::size_t nextRightOutFee = dataStr.find(kFeeSep, endP);
    std::size_t nextCoinSep = dataStr.find(kCoinSep, endP);
    if (nextRightOutFee > nextCoinSep) {
      // This means no withdraw fee data, probably 0 ?
      ret.insert_or_assign(coinAcro, MonetaryAmount(0, coinAcro));
      continue;
    }
    charPos = dataStr.find(kFeeSep, endP);
    if (charPos == string::npos) {
      break;
    }
    charPos = dataStr.find('>', charPos) + 1;
    endP = dataStr.find('<', charPos);
    std::string_view withdrawFee(dataStr.begin() + charPos, dataStr.begin() + endP);
    MonetaryAmount ma(withdrawFee, coinAcro);
    log::debug("Updated Bithumb withdrawal fees {}", ma);
    ret.insert_or_assign(std::move(coinAcro), std::move(ma));
  }
  if (ret.empty()) {
    log::error("Unable to parse Bithumb withdrawal fees, syntax might have changed");
  } else {
    log::info("Updated Bithumb withdrawal fees for {} coins", ret.size());
  }
  return ret;
}

CurrencyExchangeFlatSet BithumbPublic::TradableCurrenciesFunc::operator()() {
  json result = PublicQuery(_curlHandle, "/public/assetsstatus", "all");
  CurrencyExchangeVector currencies;
  currencies.reserve(static_cast<CurrencyExchangeVector::size_type>(result.size() + 1));
  for (const auto& [asset, withdrawalDeposit] : result.items()) {
    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(asset));
    CurrencyCode exchangeCode(asset);
    CurrencyExchange newCurrency(currencyCode, exchangeCode, exchangeCode,
                                 withdrawalDeposit["deposit_status"] == 1 ? CurrencyExchange::Deposit::kAvailable
                                                                          : CurrencyExchange::Deposit::kUnavailable,
                                 withdrawalDeposit["withdrawal_status"] == 1 ? CurrencyExchange::Withdraw::kAvailable
                                                                             : CurrencyExchange::Withdraw::kUnavailable,
                                 _cryptowatchAPI.queryIsCurrencyCodeFiat(currencyCode)
                                     ? CurrencyExchange::Type::kFiat
                                     : CurrencyExchange::Type::kCrypto);

    log::debug("Retrieved Bithumb Currency {}", newCurrency.str());
    currencies.push_back(std::move(newCurrency));
  }
  currencies.emplace_back("KRW", "KRW", "KRW", CurrencyExchange::Deposit::kUnavailable,
                          CurrencyExchange::Withdraw::kUnavailable, CurrencyExchange::Type::kFiat);
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} Bithumb currencies", ret.size());
  return ret;
}

namespace {
MarketOrderBookMap GetOrderbooks(CurlHandle& curlHandle, const CoincenterInfo& config, const ExchangeInfo& exchangeInfo,
                                 std::optional<Market> optM = std::nullopt,
                                 std::optional<int> optDepth = std::nullopt) {
  MarketOrderBookMap ret;
  // 'all' seems to work as default for all public methods
  CurrencyCode base("all");
  CurrencyCode quote;
  const bool singleMarketQuote = optM.has_value();
  if (optM) {
    base = optM->base();
    quote = optM->quote();
  }
  string urlOpts;
  if (optDepth) {
    urlOpts.append("count=");
    AppendString(urlOpts, *optDepth);
  }

  json result = PublicQuery(curlHandle, "/public/orderbook", base, quote, urlOpts);

  // Note: as of 2021-02-24, Bithumb payment currency is always KRW. Format of json may change once it's not the case
  // anymore
  std::string_view quoteCurrency = result["payment_currency"].get<std::string_view>();
  if (quoteCurrency != "KRW") {
    log::error("Unexpected Bithumb reply for orderbook. May require code api update");
  }
  CurrencyCode quoteCurrencyCode(config.standardizeCurrencyCode(quoteCurrency));
  const CurrencyCodeSet& excludedCurrencies = exchangeInfo.excludedCurrenciesAll();
  for (const auto& [baseOrSpecial, asksAndBids] : result.items()) {
    if (baseOrSpecial != "payment_currency" && baseOrSpecial != "timestamp") {
      const json* asksBids[2];
      CurrencyCode baseCurrencyCode;
      if (singleMarketQuote && baseOrSpecial == "order_currency") {
        // single market quote
        baseCurrencyCode = base;
        asksBids[0] = std::addressof(result["asks"]);
        asksBids[1] = std::addressof(result["bids"]);
      } else if (!singleMarketQuote) {
        // then it's a base currency
        baseCurrencyCode = config.standardizeCurrencyCode(baseOrSpecial);
        if (excludedCurrencies.contains(baseCurrencyCode)) {
          // Forbidden currency, do not consider its market
          log::trace("Discard {} excluded by config", baseCurrencyCode);
          continue;
        }
        asksBids[0] = std::addressof(asksAndBids["asks"]);
        asksBids[1] = std::addressof(asksAndBids["bids"]);
      } else {
        continue;
      }

      /*
        "bids": [{"quantity" : "6.1189306","price" : "504000"},
                 {"quantity" : "10.35117828","price" : "503000"}],
        "asks": [{"quantity" : "2.67575", "price" : "506000"},
                 {"quantity" : "3.54343","price" : "507000"}]
      */
      using OrderBookVec = vector<OrderBookLine>;
      OrderBookVec orderBookLines;
      orderBookLines.reserve(static_cast<OrderBookVec::size_type>(asksBids[0]->size() + asksBids[1]->size()));
      for (const json* asksOrBids : asksBids) {
        const bool isAsk = asksOrBids == asksBids[0];
        for (const json& priceQuantityPair : *asksOrBids) {
          MonetaryAmount amount(priceQuantityPair["quantity"].get<std::string_view>(), baseCurrencyCode);
          MonetaryAmount price(priceQuantityPair["price"].get<std::string_view>(), quoteCurrencyCode);

          orderBookLines.emplace_back(amount, price, isAsk);
        }
      }
      Market market(baseCurrencyCode, quoteCurrencyCode);
      ret.insert_or_assign(market, MarketOrderBook(market, orderBookLines));
      if (singleMarketQuote) {
        break;
      }
    }
  }
  log::info("Retrieved {} markets (+ orderbooks) from Bithumb", ret.size());
  return ret;
}
}  // namespace

MarketOrderBookMap BithumbPublic::AllOrderBooksFunc::operator()() {
  return GetOrderbooks(_curlHandle, _coincenterInfo, _exchangeInfo);
}

MarketOrderBook BithumbPublic::OrderBookFunc::operator()(Market mk, int depth) {
  MarketOrderBookMap marketOrderBookMap = GetOrderbooks(_curlHandle, _coincenterInfo, _exchangeInfo, mk, depth);
  auto it = marketOrderBookMap.find(mk);
  if (it == marketOrderBookMap.end()) {
    throw exception("Cannot find {} in market order book map", mk);
  }
  return it->second;
}

MonetaryAmount BithumbPublic::TradedVolumeFunc::operator()(Market mk) {
  TimePoint t1 = Clock::now();
  json result = PublicQuery(_curlHandle, "/public/ticker", mk.base(), mk.quote());
  std::string_view last24hVol = result["units_traded_24H"].get<std::string_view>();
  std::string_view bithumbTimestamp = result["date"].get<std::string_view>();
  int64_t bithumbTimeMs = FromString<int64_t>(bithumbTimestamp);
  int64_t t1Ms = TimestampToMs(t1);
  int64_t t2Ms = TimestampToMs(Clock::now());
  if (t1Ms < bithumbTimeMs && bithumbTimeMs < t2Ms) {
    log::debug("Bithumb time is synchronized with us");
  } else {
    log::error("Bithumb time is not synchronized with us (Bithumb: {}, us: [{} - {}])", bithumbTimestamp, t1Ms, t2Ms);
  }

  return {last24hVol, mk.base()};
}

namespace {
TimePoint EpochTime(std::string&& dateStr) {
  std::istringstream ss(std::move(dateStr));
  std::tm tm{};
  ss >> std::get_time(&tm, kTimeYearToSecondSpaceSeparatedFormat);
  static constexpr Duration kKoreaUTCTime = std::chrono::hours(9);
  return Clock::from_time_t(std::mktime(&tm)) - kKoreaUTCTime;
}
}  // namespace

LastTradesVector BithumbPublic::queryLastTrades(Market mk, [[maybe_unused]] int nbTrades) {
  json result = PublicQuery(_curlHandle, "/public/transaction_history", mk.base(), mk.quote());
  LastTradesVector ret;
  for (const json& detail : result) {
    MonetaryAmount amount(detail["units_traded"].get<std::string_view>(), mk.base());
    MonetaryAmount price(detail["price"].get<std::string_view>(), mk.quote());
    // Korea time (UTC+9) in this format: "2021-11-29 03:29:35"
    TradeSide tradeSide = detail["type"].get<std::string_view>() == "bid" ? TradeSide::kBuy : TradeSide::kSell;

    ret.emplace_back(tradeSide, amount, price,
                     EpochTime(std::string(detail["transaction_date"].get<std::string_view>())));
  }
  std::ranges::sort(ret);
  return ret;
}

}  // namespace cct::api
