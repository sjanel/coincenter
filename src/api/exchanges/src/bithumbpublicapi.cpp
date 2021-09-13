#include "bithumbpublicapi.hpp"

#include <algorithm>
#include <cassert>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "jsonhelpers.hpp"
#include "monetaryamount.hpp"

namespace cct {
namespace api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, CurrencyCode base,
                 CurrencyCode quote = CurrencyCode::kNeutral, std::string_view urlOpts = "") {
  std::string method_url = BithumbPublic::kUrlBase;
  method_url.append("/public/");
  method_url.append(endpoint);
  method_url.push_back('/');
  method_url.append(base.str());
  if (quote != CurrencyCode::kNeutral) {
    method_url.push_back('_');
    method_url.append(quote.str());
  }
  if (!urlOpts.empty()) {
    method_url.push_back('?');
    method_url.append(urlOpts);
  }

  CurlOptions opts(CurlOptions::RequestType::kGet);
  opts.userAgent = BithumbPublic::kUserAgent;

  json dataJson = json::parse(curlHandle.query(method_url, opts));
  if (dataJson.contains("status")) {
    std::string_view statusCode = dataJson["status"].get<std::string_view>();  // "5300" for instance
    if (statusCode != "0000") {                                                // "0000" stands for: request OK
      std::string msg;
      if (dataJson.contains("message")) {
        msg = dataJson["message"];
      }
      throw exception("error: " + std::string(statusCode) + " \"" + msg + "\"");
    }
  }
  return dataJson["data"];
}

}  // namespace

BithumbPublic::BithumbPublic(CoincenterInfo& config, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("bithumb", fiatConverter, cryptowatchAPI, config),
      _curlHandle(config.exchangeInfo(_name).minPublicQueryDelay(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault), config,
          _curlHandle),
      _withdrawalFeesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kWithdrawalFees), _cachedResultVault)),
      _allOrderBooksCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAllOrderBooks), _cachedResultVault),
          config, _curlHandle, config.exchangeInfo(_name)),
      _orderbookCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kOrderBook), _cachedResultVault), config,
          _curlHandle, config.exchangeInfo(_name)),
      _tradedVolumeCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kTradedVolume), _cachedResultVault),
          _curlHandle) {}

ExchangePublic::MarketSet BithumbPublic::queryTradableMarkets() {
  const MarketOrderBookMap& marketOrderBookMap = _allOrderBooksCache.get();
  MarketSet markets;
  markets.reserve(static_cast<MarketSet::size_type>(marketOrderBookMap.size()));
  std::transform(marketOrderBookMap.begin(), marketOrderBookMap.end(), std::inserter(markets, markets.end()),
                 [](const auto& it) { return it.first; });
  return markets;
}

MonetaryAmount BithumbPublic::queryLastPrice(Market m) {
  // Bithumb does not have a REST API endpoint for last price, let's compute it from the orderbook
  std::optional<MonetaryAmount> avgPrice = queryOrderBook(m).averagePrice();
  if (!avgPrice) {
    log::error("Empty order book for {} on {} cannot compute average price", m.str(), _name);
    return MonetaryAmount(0, m.quote(), 0);
  }
  return *avgPrice;
}

ExchangePublic::WithdrawalFeeMap BithumbPublic::WithdrawalFeesFunc::operator()() {
  WithdrawalFeeMap ret;
  // This is not a published API and only a "standard" html page. We will capture the text information in it.
  // Warning, it's not in json format so we will need manual parsing.
  constexpr char kInfoFeeUrl[] = "https://en.bithumb.com/customer_support/info_fee";
  CurlOptions opts(CurlOptions::RequestType::kGet);
  std::string s = _curlHandle.query(kInfoFeeUrl, opts);
  // Now, we have the big string containing the html data. The following should work as long as format is unchanged.
  // here is a line containing our coin with its additional withdrawal fees:
  //
  // <tr data-coin="Bitcoin"><td class="money_type tx_c">Bitcoin(BTC)</td><td id="COIN_C0101"><div
  // class="right"></div></td><td><div class="right out_fee">0.001</div></td></tr><tr data-coin="Ethereum"><td
  // class="money_type tx_c">
  constexpr std::string_view coinSep = "tr data-coin=";
  constexpr std::string_view feeSep = "right out_fee";
  for (std::size_t p = s.find(coinSep); p != std::string::npos; p = s.find(coinSep, p)) {
    p = s.find("money_type tx_c", p);
    p = s.find('(', p) + 1;
    std::size_t endP = s.find(')', p);
    CurrencyCode coinAcro(std::string_view(s.begin() + p, s.begin() + endP));
    std::size_t nextRightOutFee = s.find(feeSep, endP);
    std::size_t nextCoinSep = s.find(coinSep, endP);
    if (nextRightOutFee > nextCoinSep) {
      // This means no withdraw fee data, probably 0 ?
      MonetaryAmount ma("0", coinAcro);
      ret.insert_or_assign(coinAcro, ma);
      continue;
    }
    p = s.find(feeSep, endP);
    if (p == std::string::npos) {
      break;
    }
    p = s.find('>', p) + 1;
    endP = s.find('<', p);
    std::string_view withdrawFee(s.begin() + p, s.begin() + endP);
    MonetaryAmount ma(withdrawFee, coinAcro);
    log::debug("Updated Bithumb withdrawal fees {}", ma.str());
    ret.insert_or_assign(coinAcro, ma);
  }
  if (ret.empty()) {
    log::error("Unable to parse Bithumb withdrawal fees, probably syntax has changed");
  } else {
    log::info("Updated Bithumb withdrawal fees for {} coins", ret.size());
  }
  return ret;
}

CurrencyExchangeFlatSet BithumbPublic::TradableCurrenciesFunc::operator()() {
  json result = PublicQuery(_curlHandle, "assetsstatus", "all");
  CurrencyExchangeFlatSet currencies;
  currencies.reserve(static_cast<CurrencyExchangeFlatSet::size_type>(result.size() + 1));
  for (const auto& [asset, withdrawalDeposit] : result.items()) {
    CurrencyCode currencyCode(_config.standardizeCurrencyCode(asset));
    CurrencyCode exchangeCode(asset);
    CurrencyExchange newCurrency(currencyCode, exchangeCode, exchangeCode,
                                 withdrawalDeposit["deposit_status"] == 1 ? CurrencyExchange::Deposit::kAvailable
                                                                          : CurrencyExchange::Deposit::kUnavailable,
                                 withdrawalDeposit["withdrawal_status"] == 1
                                     ? CurrencyExchange::Withdraw::kAvailable
                                     : CurrencyExchange::Withdraw::kUnavailable);
    if (currencies.contains(newCurrency)) {
      log::error("Duplicated {}", newCurrency.str());
    } else {
      log::debug("Retrieved Bithumb Currency {}", newCurrency.str());
      currencies.insert(std::move(newCurrency));
    }
  }
  currencies.emplace("KRW", "KRW", "KRW", CurrencyExchange::Deposit::kUnavailable,
                     CurrencyExchange::Withdraw::kUnavailable);
  log::info("Retrieved {} Bithumb currencies", currencies.size());
  return currencies;
}

namespace {
ExchangePublic::MarketOrderBookMap GetOrderbooks(CurlHandle& curlHandle, CoincenterInfo& config,
                                                 const ExchangeInfo& exchangeInfo,
                                                 std::optional<Market> optM = std::nullopt,
                                                 std::optional<int> optDepth = std::nullopt) {
  ExchangePublic::MarketOrderBookMap ret;
  // all seems to work as default for all public methods
  CurrencyCode base("all");
  CurrencyCode quote(CurrencyCode::kNeutral);
  const bool singleMarketQuote = optM.has_value();
  if (optM) {
    base = optM->base();
    quote = optM->quote();
  }
  std::string urlOpts;
  if (optDepth) {
    urlOpts = std::string("count=").append(std::to_string(*optDepth));
  }

  json result = PublicQuery(curlHandle, "orderbook", base, quote, urlOpts);

  // Note: as of 2021-02-24, Bithumb payment currency is always KRW. Format of json may change once it's not the case
  // anymore
  std::string quoteCurrency = result["payment_currency"];
  if (quoteCurrency != "KRW") {
    log::error("Unexpected Bithumb reply for orderbook. May require code api update");
  }
  CurrencyCode quoteCurrencyCode(config.standardizeCurrencyCode(quoteCurrency));
  const ExchangeInfo::CurrencySet& excludedCurrencies = exchangeInfo.excludedCurrenciesAll();
  for (const auto& [baseOrSpecial, asksAndBids] : result.items()) {
    std::string baseOrSpecialStr = baseOrSpecial;
    if (baseOrSpecialStr != "payment_currency" && baseOrSpecialStr != "timestamp") {
      const json* asksBids[2];
      CurrencyCode baseCurrencyCode;
      if (singleMarketQuote && baseOrSpecialStr == "order_currency") {
        // single market quote
        baseCurrencyCode = base;
        asksBids[0] = std::addressof(result["asks"]);
        asksBids[1] = std::addressof(result["bids"]);
      } else if (!singleMarketQuote) {
        // then it's a base currency
        baseCurrencyCode = CurrencyCode(config.standardizeCurrencyCode(baseOrSpecialStr));
        if (excludedCurrencies.contains(baseCurrencyCode)) {
          // Forbidden currency, do not consider its market
          log::trace("Discard {} excluded by config", baseCurrencyCode.str());
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
      using OrderBookVec = cct::vector<OrderBookLine>;
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

ExchangePublic::MarketOrderBookMap BithumbPublic::AllOrderBooksFunc::operator()(int) {
  return GetOrderbooks(_curlHandle, _config, _exchangeInfo);
}

MarketOrderBook BithumbPublic::OrderBookFunc::operator()(Market m, int count) {
  ExchangePublic::MarketOrderBookMap marketOrderBookMap = GetOrderbooks(_curlHandle, _config, _exchangeInfo, m, count);
  auto it = marketOrderBookMap.find(m);
  if (it == marketOrderBookMap.end()) {
    throw exception("Unexpected answer from get OrderBooks");
  }
  return it->second;
}

MonetaryAmount BithumbPublic::TradedVolumeFunc::operator()(Market m) {
  json result = PublicQuery(_curlHandle, "ticker", m.base(), m.quote());
  std::string_view last24hVol = result["units_traded_24H"].get<std::string_view>();
  return MonetaryAmount(last24hVol, m.base());
}

}  // namespace api
}  // namespace cct
