#include "huobipublicapi.hpp"

#include <algorithm>
#include <cassert>
#include <execution>
#include <thread>
#include <unordered_map>

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_toupperlower.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "monetaryamount.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view endpoint, const CurlPostData& curlPostData = CurlPostData()) {
  std::string url = HuobiPublic::kUrlBase;
  url.push_back('/');
  url.append(endpoint);
  if (!curlPostData.empty()) {
    url.push_back('?');
    url.append(curlPostData.toStringView());
  }
  CurlOptions opts(CurlOptions::RequestType::kGet);
  opts.userAgent = HuobiPublic::kUserAgent;
  json dataJson = json::parse(curlHandle.query(url, opts));
  bool returnData = dataJson.contains("data");
  if (!returnData && !dataJson.contains("tick")) {
    throw exception("No data for Huobi public endpoint");
  }
  return returnData ? dataJson["data"] : dataJson["tick"];
}

}  // namespace

HuobiPublic::HuobiPublic(CoincenterInfo& config, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("huobi", fiatConverter, cryptowatchAPI, config),
      _exchangeInfo(config.exchangeInfo(_name)),
      _curlHandle(_exchangeInfo.minPublicQueryDelay(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault),
          _curlHandle),
      _marketsCache(CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kMarkets), _cachedResultVault),
                    config, _curlHandle, _exchangeInfo),
      _allOrderBooksCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAllOrderBooks), _cachedResultVault),
          _marketsCache, _curlHandle, _exchangeInfo),
      _orderbookCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kOrderBook), _cachedResultVault), config,
          _curlHandle, _exchangeInfo) {}

json HuobiPublic::TradableCurrenciesFunc::operator()() { return PublicQuery(_curlHandle, "v2/reference/currencies"); }

CurrencyExchangeFlatSet HuobiPublic::queryTradableCurrencies() {
  const json& result = _tradableCurrenciesCache.get();
  CurrencyExchangeFlatSet currencies;
  currencies.reserve(static_cast<CurrencyExchangeFlatSet::size_type>(result.size()));
  for (const json& curDetail : result) {
    std::string_view statusStr = curDetail["instStatus"].get<std::string_view>();
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    if (statusStr != "normal") {
      log::debug("{} is {} from Huobi", curStr, statusStr);
      continue;
    }
    bool foundChainWithSameName = false;
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    for (const json& chainDetail : curDetail["chains"]) {
      std::string_view chainName = chainDetail["chain"].get<std::string_view>();
      std::string_view displayName = chainDetail["displayName"].get<std::string_view>();
      if (CurrencyCode(chainName) != cur && CurrencyCode(displayName) != cur) {
        log::debug("Discarding chain {}", chainName);
        continue;
      }
      std::string_view depositAllowedStr = chainDetail["depositStatus"].get<std::string_view>();
      std::string_view withdrawAllowedStr = chainDetail["withdrawStatus"].get<std::string_view>();
      CurrencyExchange newCurrency(cur, curStr, curStr,
                                   depositAllowedStr == "allowed" ? CurrencyExchange::Deposit::kAvailable
                                                                  : CurrencyExchange::Deposit::kUnavailable,
                                   withdrawAllowedStr == "allowed" ? CurrencyExchange::Withdraw::kAvailable
                                                                   : CurrencyExchange::Withdraw::kUnavailable);
      if (currencies.contains(newCurrency)) {
        log::error("Duplicated {}", newCurrency.str());
      } else {
        log::debug("Retrieved Huobi Currency {}", newCurrency.str());
        currencies.insert(std::move(newCurrency));
      }
      foundChainWithSameName = true;
      break;
    }
    if (!foundChainWithSameName) {
      log::debug("Cannot find {} main chain in Huobi, discarding currency", cur.str());
    }
  }
  log::info("Retrieved {} Huobi currencies", currencies.size());
  return currencies;
}

std::pair<ExchangePublic::MarketSet, HuobiPublic::MarketsFunc::MarketInfoMap> HuobiPublic::MarketsFunc::operator()() {
  json result = PublicQuery(_curlHandle, "v1/common/symbols");

  MarketSet markets;
  MarketInfoMap marketInfoMap;

  markets.reserve(static_cast<MarketSet::size_type>(result.size()));

  const ExchangeInfo::CurrencySet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();

  for (const json& marketDetails : result) {
    std::string_view baseAsset = marketDetails["base-currency"].get<std::string_view>();
    std::string_view quoteAsset = marketDetails["quote-currency"].get<std::string_view>();
    if (excludedCurrencies.contains(baseAsset) || excludedCurrencies.contains(quoteAsset)) {
      log::trace("Discard {}-{} excluded by config", baseAsset, quoteAsset);
      continue;
    }
    std::string_view apiTradingStr = marketDetails["api-trading"].get<std::string_view>();
    if (apiTradingStr != "enabled") {
      log::trace("Trading is {} for market {}-{}", baseAsset, quoteAsset);
      continue;
    }
    if (baseAsset.size() > CurrencyCode::kAcronymMaxLen || quoteAsset.size() > CurrencyCode::kAcronymMaxLen) {
      log::trace("Discard {}-{} as one asset is too long", baseAsset, quoteAsset);
      continue;
    }
    log::trace("Accept {}-{} Huobi asset pair", baseAsset, quoteAsset);
    CurrencyCode base(baseAsset);
    CurrencyCode quote(quoteAsset);
    Market m(base, quote);
    markets.insert(m);

    int8_t volNbDec = marketDetails["amount-precision"].get<int8_t>();
    int8_t priNbDec = marketDetails["price-precision"].get<int8_t>();
    // value = vol * pri in quote currency
    MarketInfo marketInfo;

    marketInfo.volAndPriNbDecimals = VolAndPriNbDecimals(volNbDec, priNbDec);

    marketInfo.minOrderValue = MonetaryAmount(marketDetails["min-order-value"].get<double>(), m.quote());
    if (marketDetails.contains("max-order-value")) {  // in USDT
      marketInfo.maxOrderValueUSDT = MonetaryAmount(marketDetails["max-order-value"].get<double>(), "USDT");
    }

    marketInfo.limitMinOrderAmount = MonetaryAmount(marketDetails["limit-order-min-order-amt"].get<double>(), m.base());
    marketInfo.limitMaxOrderAmount = MonetaryAmount(marketDetails["limit-order-max-order-amt"].get<double>(), m.base());

    marketInfo.sellMarketMinOrderAmount =
        MonetaryAmount(marketDetails["sell-market-min-order-amt"].get<double>(), m.base());
    marketInfo.sellMarketMaxOrderAmount =
        MonetaryAmount(marketDetails["sell-market-max-order-amt"].get<double>(), m.base());

    marketInfo.buyMarketMaxOrderValue =
        MonetaryAmount(marketDetails["buy-market-max-order-value"].get<double>(), m.quote());

    marketInfoMap.insert_or_assign(m, std::move(marketInfo));
  }
  log::info("Retrieved huobi {} markets", markets.size());
  return {std::move(markets), std::move(marketInfoMap)};
}

ExchangePublic::WithdrawalFeeMap HuobiPublic::queryWithdrawalFees() {
  WithdrawalFeeMap ret;
  for (const json& curDetail : _tradableCurrenciesCache.get()) {
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    bool foundChainWithSameName = false;
    for (const json& chainDetail : curDetail["chains"]) {
      std::string_view chainName = chainDetail["chain"].get<std::string_view>();
      std::string_view displayName = chainDetail["displayName"].get<std::string_view>();
      if (CurrencyCode(chainName) != cur && CurrencyCode(displayName) != cur) {
        log::debug("Discarding chain '{}'", chainName);
        continue;
      }
      std::string_view withdrawFeeStr = chainDetail["transactFeeWithdraw"].get<std::string_view>();
      MonetaryAmount withdrawFee(withdrawFeeStr, cur);

      log::trace("Retrieved {} withdrawal fee {}", _name, withdrawFee.str());
      ret.insert_or_assign(cur, withdrawFee);

      foundChainWithSameName = true;
      break;
    }
    if (!foundChainWithSameName) {
      log::debug("Cannot find '{}' main chain in {}, discarding currency", curStr, _name);
    }
  }

  log::info("Retrieved {} withdrawal fees for {} coins", _name, ret.size());
  assert(!ret.empty());
  return ret;
}

MonetaryAmount HuobiPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  for (const json& curDetail : _tradableCurrenciesCache.get()) {
    std::string_view curStr = curDetail["currency"].get<std::string_view>();
    CurrencyCode cur(_coincenterInfo.standardizeCurrencyCode(curStr));
    if (cur != currencyCode) {
      continue;
    }
    for (const json& chainDetail : curDetail["chains"]) {
      std::string_view chainName = chainDetail["chain"].get<std::string_view>();
      if (chainName == cur) {
        return MonetaryAmount(chainDetail["transactFeeWithdraw"].get<std::string_view>(), cur);
      }
    }
  }
  throw exception("Unable to find withdrawal fee for " + std::string(currencyCode.str()));
}

ExchangePublic::MarketOrderBookMap HuobiPublic::AllOrderBooksFunc::operator()(int depth) {
  MarketOrderBookMap ret;
  const auto& [markets, marketInfoMap] = _marketsCache.get();
  using HuobiAssetPairToStdMarketMap = std::unordered_map<std::string, Market>;
  HuobiAssetPairToStdMarketMap huobiAssetPairToStdMarketMap;
  huobiAssetPairToStdMarketMap.reserve(markets.size());
  for (Market m : markets) {
    std::string upperMarket = cct::toupper(m.assetsPairStr());
    huobiAssetPairToStdMarketMap.insert_or_assign(std::move(upperMarket), m);
  }
  for (const json& tickerDetails : PublicQuery(_curlHandle, "market/tickers")) {
    std::string upperMarket = cct::toupper(tickerDetails["symbol"].get<std::string_view>());
    auto it = huobiAssetPairToStdMarketMap.find(upperMarket);
    if (it == huobiAssetPairToStdMarketMap.end()) {
      continue;
    }
    Market m = it->second;
    MonetaryAmount askPri(tickerDetails["ask"].get<double>(), m.quote());
    MonetaryAmount bidPri(tickerDetails["bid"].get<double>(), m.quote());
    MonetaryAmount askVol(tickerDetails["askSize"].get<double>(), m.base());
    MonetaryAmount bidVol(tickerDetails["bidSize"].get<double>(), m.base());

    if (bidVol.isZero() || askVol.isZero()) {
      log::trace("No volume for {}", m.assetsPairStr());
      continue;
    }

    const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(m)->second;

    ret.insert_or_assign(m, MarketOrderBook(askPri, askVol, bidPri, bidVol, marketInfo.volAndPriNbDecimals, depth));
  }

  log::info("Retrieved Huobi ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook HuobiPublic::OrderBookFunc::operator()(Market m, int depth) {
  // Huobi has a fixed range of authorized values for depth
  std::string lowerCaseAssets = cct::tolower(m.assetsPairStr());
  CurlPostData postData{{"symbol", std::string_view(lowerCaseAssets)}, {"type", "step0"}};
  if (depth != kHuobiStandardOrderBookDefaultDepth) {
    constexpr int kAuthorizedDepths[] = {5, 10, 20};
    auto lb = std::lower_bound(std::begin(kAuthorizedDepths), std::end(kAuthorizedDepths), depth);
    if (lb == std::end(kAuthorizedDepths)) {
      lb = std::next(std::end(kAuthorizedDepths), -1);
      log::error("Invalid depth {}, default to {}", kHuobiStandardOrderBookDefaultDepth);
    } else {
      postData.append("depth", std::to_string(*lb));
    }
  }
  json asksAndBids = PublicQuery(_curlHandle, "market/depth", postData);
  const json& asks = asksAndBids["asks"];
  const json& bids = asksAndBids["bids"];
  using OrderBookVec = cct::vector<OrderBookLine>;
  OrderBookVec orderBookLines;
  orderBookLines.reserve(static_cast<OrderBookVec::size_type>(asks.size() + bids.size()));
  for (auto asksOrBids : {std::addressof(asks), std::addressof(bids)}) {
    const bool isAsk = asksOrBids == std::addressof(asks);
    for (const auto& priceQuantityPair : *asksOrBids) {
      MonetaryAmount amount(priceQuantityPair.back().get<double>(), m.base());
      MonetaryAmount price(priceQuantityPair.front().get<double>(), m.quote());

      orderBookLines.emplace_back(amount, price, isAsk);
    }
  }
  return MarketOrderBook(m, orderBookLines);
}

MonetaryAmount HuobiPublic::sanitizePrice(Market m, MonetaryAmount pri) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  pri.truncate(marketInfoMap.find(m)->second.volAndPriNbDecimals.priNbDecimals);
  return pri;
}
MonetaryAmount HuobiPublic::sanitizeVolume(Market m, CurrencyCode fromCurrencyCode, MonetaryAmount vol,
                                           MonetaryAmount sanitizedPrice, bool isTakerOrder) {
  const MarketsFunc::MarketInfoMap& marketInfoMap = _marketsCache.get().second;
  const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(m)->second;
  MonetaryAmount sanitizedVol = vol;

  if (sanitizedVol.toNeutral() * sanitizedPrice < marketInfo.minOrderValue) {
    sanitizedVol = MonetaryAmount(marketInfo.minOrderValue / sanitizedPrice, sanitizedVol.currencyCode());
    MonetaryAmount step(1, sanitizedVol.currencyCode(), marketInfo.volAndPriNbDecimals.volNbDecimals);
    sanitizedVol = sanitizedVol.round(step, MonetaryAmount::RoundType::kUp);
  } else {
    sanitizedVol.truncate(marketInfo.volAndPriNbDecimals.volNbDecimals);
  }
  if (isTakerOrder) {
    if (fromCurrencyCode == m.base() && sanitizedVol < marketInfo.sellMarketMinOrderAmount) {
      sanitizedVol = marketInfo.sellMarketMinOrderAmount;
    }
  } else {
    if (sanitizedVol < marketInfo.limitMinOrderAmount) {
      sanitizedVol = marketInfo.limitMinOrderAmount;
    }
  }
  if (sanitizedVol != vol) {
    log::warn("Sanitize volume {} -> {}", vol.str(), sanitizedVol.str());
  }
  return sanitizedVol;
}

}  // namespace api
}  // namespace cct
