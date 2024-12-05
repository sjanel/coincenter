#include "bithumbpublicapi.hpp"

#include <algorithm>
#include <amc/isdetected.hpp>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "apiquerytypeenum.hpp"
#include "bithumb-schema.hpp"
#include "cachedresult.hpp"
#include "cct_const.hpp"
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
#include "read-json.hpp"
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

template <class T>
T PublicQuery(CurlHandle& curlHandle, std::string_view method, CurrencyCode base, CurrencyCode quote = CurrencyCode(),
              std::string_view urlOpts = "") {
  RequestRetry requestRetry(curlHandle, CurlOptions(HttpRequestType::kGet));

  return requestRetry.query<T, json::opts{.error_on_unknown_keys = false, .minified = true, .raw_string = true}>(
      ComputeMethodUrl(method, base, quote, urlOpts), [](const T& response) {
        if constexpr (amc::is_detected<has_status_t, T>::value) {
          if (!response.status.empty()) {
            auto statusCode = StringToIntegral<int64_t>(response.status);
            if (statusCode != BithumbPublic::kStatusOK) {
              log::warn("Bithumb error ({})", statusCode);
              return RequestRetry::Status::kResponseError;
            }
          }
        }

        return RequestRetry::Status::kResponseOK;
      });
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

bool BithumbPublic::healthCheck() {
  auto networkInfoStr = _curlHandle.query("/public/network-info", CurlOptions(HttpRequestType::kGet));
  schema::bithumb::V1NetworkInfo networkInfo;
  auto ec = ReadJson<json::opts{.error_on_unknown_keys = false, .minified = true, .raw_string = true}>(
      networkInfoStr, "Bithumb network info", networkInfo);
  if (ec) {
    log::error("{} health check response is badly formatted", name());
    return false;
  }
  const auto statusCode = StringToIntegral<int64_t>(networkInfo.status);
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
  return _commonApi.tryQueryWithdrawalFee(exchangeNameEnum(), currencyCode);
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
  auto result = PublicQuery<schema::bithumb::V1AssetStatus>(_curlHandle, "/public/assetsstatus/", "all");

  CurrencyExchangeVector currencies;
  currencies.reserve(static_cast<CurrencyExchangeVector::size_type>(result.data.size() + 1));

  for (const auto& [asset, curData] : result.data) {
    CurrencyCode currencyCode(_coincenterInfo.standardizeCurrencyCode(asset));
    CurrencyCode exchangeCode(asset);
    CurrencyExchange newCurrency(
        currencyCode, exchangeCode, exchangeCode,
        curData.deposit_status == 1 ? CurrencyExchange::Deposit::kAvailable : CurrencyExchange::Deposit::kUnavailable,
        curData.withdrawal_status == 1 ? CurrencyExchange::Withdraw::kAvailable
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
void ParseOrderBookLines(const schema::bithumb::OrderbookData& data, Market mk, int depth,
                         MarketOrderBookLines& orderBookLines) {
  orderBookLines.clear();
  orderBookLines.reserve(std::min<MarketOrderBookLines::size_type>(2 * depth, data.asks.size() + data.bids.size()));
  for (const auto asksOrBids : {&data.asks, &data.bids}) {
    const auto type = asksOrBids == &data.asks ? OrderBookLine::Type::kAsk : OrderBookLine::Type::kBid;
    for (const auto& order : *asksOrBids | std::ranges::views::take(depth)) {
      MonetaryAmount price(order.price, mk.quote());
      MonetaryAmount amount(order.quantity, mk.base());

      orderBookLines.push(amount, price, type);
    }
  }
}
}  // namespace

MarketOrderBookMap BithumbPublic::AllOrderBooksFunc::operator()() {
  CurrencyCode base("ALL");
  CurrencyCode quote;

  auto result = PublicQuery<schema::bithumb::MultiOrderbook>(_curlHandle, "/public/orderbook/", base, quote);
  const auto nowTime = Clock::now();
  MarketOrderBookMap ret;

  auto paymentCurrencyIt = result.data.find("payment_currency");
  if (paymentCurrencyIt == result.data.end()) {
    log::error("Unexepected Bithumb reply for orderbook. May require code api update");
    return ret;
  }

  std::visit(
      [&quote](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, string>) {
          quote = CurrencyCode{val};
        } else if constexpr (std::is_same_v<T, schema::bithumb::OrderbookData>) {
          log::error("Unexpected Bithumb reply for orderbook. May require code api update");
        } else {
          static_assert(std::is_same_v<T, string> || std::is_same_v<T, schema::bithumb::OrderbookData>,
                        "non-exhaustive visitor!");
        }
      },
      paymentCurrencyIt->second);

  if (quote.isNeutral()) {
    log::error("Unexpected payment currency {} Bithumb reply for orderbook. May require code api update", quote);
    return ret;
  }

  const CurrencyCodeSet& excludedCurrencies = _assetConfig.allExclude;

  if (excludedCurrencies.contains(quote)) {
    // Forbidden currency, do not consider its market
    log::trace("Discard {} excluded by config", quote);
    return ret;
  }

  MarketOrderBookLines orderBookLines;

  for (const auto& [key, var] : result.data) {
    std::visit(
        [quote, nowTime, &key, &excludedCurrencies, &orderBookLines, &ret](auto&& val) {
          using T = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<T, string>) {
            // do nothing, unused field
          } else if constexpr (std::is_same_v<T, schema::bithumb::OrderbookData>) {
            CurrencyCode base{key};
            if (excludedCurrencies.contains(base)) {
              // Forbidden currency, do not consider its market
              log::trace("Discard {} excluded by config", base);
              return;
            }
            Market mk(base, quote);
            ParseOrderBookLines(val, mk, 1, orderBookLines);
            ret[mk] = MarketOrderBook(nowTime, mk, orderBookLines);
          } else {
            static_assert(std::is_same_v<T, string> || std::is_same_v<T, schema::bithumb::OrderbookData>,
                          "non-exhaustive visitor!");
          }
        },
        var);
  }
  if (ret.size() > 1) {
    log::info("Retrieved {} markets (+ order books) from Bithumb", ret.size());
  }
  return ret;
}

MarketOrderBook BithumbPublic::OrderBookFunc::operator()(Market mk, int depth) {
  string urlOpts("count=");
  AppendIntegralToString(urlOpts, depth);

  auto result =
      PublicQuery<schema::bithumb::SingleOrderbook>(_curlHandle, "/public/orderbook/", mk.base(), mk.quote(), urlOpts);
  //  Note: as of 2021-02-24, Bithumb payment currency is always KRW. Format of json may change once it's not the case
  //  anymore
  if (result.data.payment_currency.isDefined() && result.data.payment_currency != "KRW") {
    log::error("Unexpected payment currency {} Bithumb reply for orderbook. May require code api update",
               result.data.payment_currency);
  }

  MarketOrderBookLines orderBookLines;

  ParseOrderBookLines(result.data, mk, depth, orderBookLines);

  return MarketOrderBook(Clock::now(), mk, orderBookLines);
}

MonetaryAmount BithumbPublic::TradedVolumeFunc::operator()(Market mk) {
  TimePoint t1 = Clock::now();
  auto result = PublicQuery<schema::bithumb::Ticker>(_curlHandle, "/public/ticker/", mk.base(), mk.quote());
  std::string_view bithumbTimestamp = result.data.date;

  int64_t bithumbTimeMs = StringToIntegral<int64_t>(bithumbTimestamp);
  int64_t t1Ms = TimestampToMillisecondsSinceEpoch(t1);
  int64_t t2Ms = TimestampToMillisecondsSinceEpoch(Clock::now());
  if (t1Ms < bithumbTimeMs && bithumbTimeMs < t2Ms) {
    log::debug("Bithumb time is synchronized with us");
  } else {
    log::error("Bithumb time is not synchronized with us (Bithumb: {}, us: [{} - {}])", bithumbTimestamp, t1Ms, t2Ms);
  }

  return {result.data.units_traded_24H, mk.base()};
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

  auto result = PublicQuery<schema::bithumb::TransactionHistory>(_curlHandle, "/public/transaction_history/", mk.base(),
                                                                 mk.quote(), urlOpts);

  PublicTradeVector ret;

  ret.reserve(static_cast<PublicTradeVector::size_type>(result.data.size()));
  for (const auto& detail : result.data) {
    MonetaryAmount amount(detail.units_traded, mk.base());
    MonetaryAmount price(detail.price, mk.quote());
    // Korea time (UTC+9) in this format: "2021-11-29 03:29:35"
    TradeSide tradeSide = detail.type == schema::bithumb::TransactionTypeEnum::bid ? TradeSide::kBuy : TradeSide::kSell;

    ret.emplace_back(tradeSide, amount, price, EpochTime(detail.transaction_date));
  }
  std::ranges::sort(ret);
  return ret;
}

}  // namespace cct::api
