#include "bithumbpublicapi.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "cachedresult.hpp"
#include "cct_const.hpp"
#include "cct_json-container.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchange-asset-config.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "httprequesttype.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "order-book-line.hpp"
#include "permanentcurloptions.hpp"
#include "public-trade-vector.hpp"
#include "request-retry.hpp"
#include "stringconv.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradeside.hpp"

namespace cct::api {
namespace {

auto ComputeMethodUrl(std::string_view endpoint, CurrencyCode base, CurrencyCode quote, std::string_view urlOpts) {
  string methodUrl;

  methodUrl.reserve(endpoint.size() + base.size() +
                    (static_cast<string::size_type>(quote.isDefined()) * (quote.size() + 1U)) +
                    (static_cast<string::size_type>(!urlOpts.empty()) * (urlOpts.size() + 1U)));

  methodUrl.append(endpoint);

  base.appendStrTo(methodUrl);
  if (quote.isDefined()) {
    methodUrl.push_back('_');
    quote.appendStrTo(methodUrl);
  }

  if (!urlOpts.empty()) {
    methodUrl.push_back('?');
    methodUrl.append(urlOpts);
  }

  return methodUrl;
}

json::container PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, CurrencyCode base,
                            CurrencyCode quote = CurrencyCode(), std::string_view urlOpts = "") {
  RequestRetry requestRetry(curlHandle, CurlOptions(HttpRequestType::kGet));

  json::container jsonResponse =
      requestRetry.queryJson(ComputeMethodUrl(endpoint, base, quote, urlOpts), [](const json::container& data) {
        const auto statusCode = BithumbPublic::StatusCodeFromJsonResponse(data);
        if (statusCode != BithumbPublic::kStatusOK) {
          log::warn("Full Bithumb error ({}): '{}'", statusCode, data.dump());
          return RequestRetry::Status::kResponseError;
        }
        return RequestRetry::Status::kResponseOK;
      });

  const auto dataIt = jsonResponse.find("data");
  json::container ret;
  if (dataIt != jsonResponse.end()) {
    ret.swap(*dataIt);
  }
  return ret;
}

}  // namespace

BithumbPublic::BithumbPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CommonAPI& commonAPI)
    : ExchangePublic(ExchangeNameEnum::bithumb, fiatConverter, commonAPI, config),
      _curlHandle(kUrlBase, config.metricGatewayPtr(), permanentCurlOptionsBuilder().build(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::currencies).duration,
                              _cachedResultVault),
          config, commonAPI, _curlHandle),
      _allOrderBooksCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::allOrderBooks).duration,
                              _cachedResultVault),
          config, _curlHandle, exchangeConfig().asset),
      _orderbookCache(CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::orderBook).duration,
                                          _cachedResultVault),
                      config, _curlHandle, exchangeConfig().asset),
      _tradedVolumeCache(
          CachedResultOptions(exchangeConfig().query.updateFrequency.at(QueryType::tradedVolume).duration,
                              _cachedResultVault),
          _curlHandle) {}

int64_t BithumbPublic::StatusCodeFromJsonResponse(const json::container& jsonResponse) {
  const auto statusIt = jsonResponse.find("status");
  if (statusIt == jsonResponse.end()) {
    return kStatusNotPresentError;
  }
  if (statusIt->is_string()) {
    return StringToIntegral<int64_t>(statusIt->get<std::string_view>());
  }
  if (statusIt->is_number_integer()) {
    return statusIt->get<int64_t>();
  }
  log::error("Unexpected 'status' value type - not a number nor a string");
  return kStatusUnexpectedError;
}

bool BithumbPublic::healthCheck() {
  static constexpr bool kAllowExceptions = false;

  const json::container jsonResponse = json::container::parse(
      _curlHandle.query("/public/assetsstatus/BTC", CurlOptions(HttpRequestType::kGet)), nullptr, kAllowExceptions);
  if (jsonResponse.is_discarded()) {
    log::error("{} health check response is badly formatted", name());
    return false;
  }
  const auto statusCode = BithumbPublic::StatusCodeFromJsonResponse(jsonResponse);
  log::info("{} status code: {}", name(), statusCode);
  return statusCode == kStatusOK;
}

MarketSet BithumbPublic::queryTradableMarkets() {
  auto [pMarketOrderbookMap, lastUpdatedTime] = _allOrderBooksCache.retrieve();
  if (pMarketOrderbookMap == nullptr ||
      lastUpdatedTime + exchangeConfig().query.updateFrequency.at(QueryType::markets).duration < Clock::now()) {
    pMarketOrderbookMap = std::addressof(_allOrderBooksCache.get());
  }
  MarketSet markets;
  markets.reserve(static_cast<MarketSet::size_type>(pMarketOrderbookMap->size()));
  std::ranges::transform(*pMarketOrderbookMap, std::inserter(markets, markets.end()),
                         [](const auto& it) { return it.first; });
  return markets;
}

std::optional<MonetaryAmount> BithumbPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  return _commonApi.tryQueryWithdrawalFee(name(), currencyCode);
}

MonetaryAmount BithumbPublic::queryLastPrice(Market mk) {
  // Bithumb does not have a REST API endpoint for last price, let's compute it from the orderbook
  std::optional<MonetaryAmount> avgPrice = getOrderBook(mk).averagePrice();
  if (!avgPrice) {
    log::error("Empty order book for {} on {} cannot compute average price", mk, name());
    return MonetaryAmount(0, mk.quote());
  }
  return *avgPrice;
}

CurrencyExchangeFlatSet BithumbPublic::TradableCurrenciesFunc::operator()() {
  json::container result = PublicQuery(_curlHandle, "/public/assetsstatus/", "all");
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
                                 _commonAPI.queryIsCurrencyCodeFiat(currencyCode) ? CurrencyExchange::Type::kFiat
                                                                                  : CurrencyExchange::Type::kCrypto);

    log::debug("Retrieved Bithumb Currency {}", newCurrency.str());
    currencies.push_back(std::move(newCurrency));
  }
  currencies.emplace_back("KRW", "KRW", "KRW", CurrencyExchange::Deposit::kUnavailable,
                          CurrencyExchange::Withdraw::kUnavailable, CurrencyExchange::Type::kFiat);
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} bithumb currencies", ret.size());
  return ret;
}

namespace {

template <class OutputType>
OutputType GetOrderBooks(CurlHandle& curlHandle, const CoincenterInfo& config,
                         const schema::ExchangeAssetConfig& assetConfig, std::optional<Market> optM = std::nullopt,
                         std::optional<int> optDepth = std::nullopt) {
  OutputType ret;
  // 'all' seems to work as default for all public methods
  CurrencyCode base("ALL");
  CurrencyCode quote;
  const bool singleMarketQuote = optM.has_value();
  if (optM) {
    base = optM->base();
    quote = optM->quote();
  }
  string urlOpts;
  if (optDepth) {
    urlOpts.append("count=");
    AppendIntegralToString(urlOpts, *optDepth);
  }

  json::container result = PublicQuery(curlHandle, "/public/orderbook/", base, quote, urlOpts);
  if (!result.empty()) {
    //  Note: as of 2021-02-24, Bithumb payment currency is always KRW. Format of json may change once it's not the case
    //  anymore
    const auto nowTime = Clock::now();
    std::string_view quoteCurrency = result["payment_currency"].get<std::string_view>();
    if (quoteCurrency != "KRW") {
      log::error("Unexpected Bithumb reply for orderbook. May require code api update");
    }
    CurrencyCode quoteCurrencyCode(config.standardizeCurrencyCode(quoteCurrency));
    const CurrencyCodeSet& excludedCurrencies = assetConfig.allExclude;

    MarketOrderBookLines orderBookLines;

    for (const auto& [baseOrSpecial, asksAndBids] : result.items()) {
      if (baseOrSpecial != "payment_currency" && baseOrSpecial != "timestamp") {
        const json::container* asksBids[2];
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
        orderBookLines.clear();
        orderBookLines.reserve(asksBids[0]->size() + asksBids[1]->size());
        for (const json::container* asksOrBids : asksBids) {
          const auto type = asksOrBids == asksBids[0] ? OrderBookLine::Type::kAsk : OrderBookLine::Type::kBid;
          for (const json::container& priceQuantityPair : *asksOrBids) {
            MonetaryAmount amount(priceQuantityPair["quantity"].get<std::string_view>(), baseCurrencyCode);
            MonetaryAmount price(priceQuantityPair["price"].get<std::string_view>(), quoteCurrencyCode);

            orderBookLines.push(amount, price, type);
          }
        }
        Market market(baseCurrencyCode, quoteCurrencyCode);
        if constexpr (std::is_same_v<OutputType, MarketOrderBookMap>) {
          ret.insert_or_assign(market, MarketOrderBook(nowTime, market, orderBookLines));
        } else {
          ret = MarketOrderBook(nowTime, market, orderBookLines);
        }
        if (singleMarketQuote) {
          break;
        }
      }
    }
  }
  if constexpr (std::is_same_v<OutputType, MarketOrderBookMap>) {
    if (ret.size() > 1) {
      log::info("Retrieved {} markets (+ order books) from Bithumb", ret.size());
    }
  }
  return ret;
}
}  // namespace

MarketOrderBookMap BithumbPublic::AllOrderBooksFunc::operator()() {
  return GetOrderBooks<MarketOrderBookMap>(_curlHandle, _coincenterInfo, _assetConfig);
}

MarketOrderBook BithumbPublic::OrderBookFunc::operator()(Market mk, int depth) {
  return GetOrderBooks<MarketOrderBook>(_curlHandle, _coincenterInfo, _assetConfig, mk, depth);
}

MonetaryAmount BithumbPublic::TradedVolumeFunc::operator()(Market mk) {
  TimePoint t1 = Clock::now();
  json::container result = PublicQuery(_curlHandle, "/public/ticker/", mk.base(), mk.quote());
  std::string_view last24hVol;
  const auto dateIt = result.find("date");
  if (dateIt != result.end()) {
    std::string_view bithumbTimestamp = dateIt->get<std::string_view>();

    last24hVol = result["units_traded_24H"].get<std::string_view>();
    int64_t bithumbTimeMs = StringToIntegral<int64_t>(bithumbTimestamp);
    int64_t t1Ms = TimestampToMillisecondsSinceEpoch(t1);
    int64_t t2Ms = TimestampToMillisecondsSinceEpoch(Clock::now());
    if (t1Ms < bithumbTimeMs && bithumbTimeMs < t2Ms) {
      log::debug("Bithumb time is synchronized with us");
    } else {
      log::error("Bithumb time is not synchronized with us (Bithumb: {}, us: [{} - {}])", bithumbTimestamp, t1Ms, t2Ms);
    }
  }

  return {last24hVol, mk.base()};
}

namespace {
TimePoint EpochTime(std::string_view dateStr) {
  // In C++26, std::istringstream can be built from a std::string_view
  std::istringstream ss(std::string{dateStr});
  std::tm tm{};
  ss >> std::get_time(&tm, kTimeYearToSecondSpaceSeparatedFormat);
  static constexpr Duration kKoreaUTCTime = std::chrono::hours(9);
  return Clock::from_time_t(std::mktime(&tm)) - kKoreaUTCTime;
}
}  // namespace

PublicTradeVector BithumbPublic::queryLastTrades(Market mk, int nbTrades) {
  static constexpr auto kNbMinLastTrades = 1;
  static constexpr auto kNbMaxLastTrades = 100;

  if (nbTrades < kNbMinLastTrades) {
    log::warn("Minimum number of last trades to ask on {} is {}", name(), kNbMinLastTrades);
    nbTrades = kNbMinLastTrades;
  } else if (nbTrades > kNbMaxLastTrades) {
    log::warn("Maximum number of last trades to ask on {} is {}", name(), kNbMaxLastTrades);
    nbTrades = kNbMaxLastTrades;
  }

  string urlOpts("count=");
  AppendIntegralToString(urlOpts, nbTrades);

  json::container result = PublicQuery(_curlHandle, "/public/transaction_history/", mk.base(), mk.quote(), urlOpts);

  PublicTradeVector ret;

  ret.reserve(static_cast<PublicTradeVector::size_type>(result.size()));
  for (const json::container& detail : result) {
    MonetaryAmount amount(detail["units_traded"].get<std::string_view>(), mk.base());
    MonetaryAmount price(detail["price"].get<std::string_view>(), mk.quote());
    // Korea time (UTC+9) in this format: "2021-11-29 03:29:35"
    TradeSide tradeSide = detail["type"].get<std::string_view>() == "bid" ? TradeSide::kBuy : TradeSide::kSell;

    ret.emplace_back(tradeSide, amount, price, EpochTime(detail["transaction_date"].get<std::string_view>()));
  }
  std::ranges::sort(ret);
  return ret;
}

}  // namespace cct::api
