#include "krakenpublicapi.hpp"

#include <cassert>
#include <unordered_map>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "jsonhelpers.hpp"

namespace cct {
namespace api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view method, CurlPostData&& postData = CurlPostData()) {
  std::string method_url = KrakenPublic::kUrlBase;
  method_url.push_back('/');
  method_url.push_back(KrakenPublic::kVersion);
  method_url.append("/public/");
  method_url.append(method);

  CurlOptions opts(CurlOptions::RequestType::kGet, std::move(postData), KrakenPublic::kUserAgent);
  std::string ret = curlHandle.query(method_url, opts);
  json jsonData = json::parse(std::move(ret));
  if (jsonData.contains("error") && !jsonData["error"].empty()) {
    throw exception("Kraken public query error: " + std::string(jsonData["error"].front()));
  }
  return jsonData["result"];
}

bool CheckCurrencyExchange(std::string_view krakenEntryCurrencyCode, std::string_view krakenAltName,
                           const ExchangeInfo::CurrencySet& excludedCurrencies, CoincenterInfo& config) {
  if (krakenAltName.ends_with(".HOLD")) {
    // These are special tokens for holding
    log::trace("Discard {} which are special tokens for holding process", krakenAltName);
    return false;
  }
  if (krakenAltName.ends_with(".M")) {
    log::trace("Discard {} which are special tokens for margin", krakenAltName);
    return false;
  }
  if (krakenAltName.ends_with(".S")) {
    log::trace("Discard {} which are special tokens for staking", krakenAltName);
    return false;
  }

  // Kraken manages 2 versions of Augur, do not take first version into account to avoid issues of acronym names
  // between exchanges
  constexpr bool kAvoidAugurV1AndKeepAugurV2 = true;

  if constexpr (kAvoidAugurV1AndKeepAugurV2) {
    if (krakenEntryCurrencyCode == "XREP") {
      log::trace("Discard {} favored by Augur V2", krakenEntryCurrencyCode);
      return false;
    }
  }

  CurrencyCode standardCode(config.standardizeCurrencyCode(krakenAltName));
  if (excludedCurrencies.contains(standardCode)) {
    // Forbidden currency, do not consider its market
    log::trace("Discard {} excluded by config", standardCode.str());
    return false;
  }
  return true;
}

}  // namespace

KrakenPublic::KrakenPublic(CoincenterInfo& config, FiatConverter& fiatConverter, CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("kraken", fiatConverter, cryptowatchAPI, config),
      _curlHandle(config.exchangeInfo(_name).minPublicQueryDelay(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault), config,
          config.exchangeInfo(_name), _curlHandle),
      _withdrawalFeesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kWithdrawalFees), _cachedResultVault),
          _name),
      _marketsCache(CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kMarkets), _cachedResultVault),
                    config, _tradableCurrenciesCache, _curlHandle, config.exchangeInfo(_name)),
      _allOrderBooksCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAllOrderBooks), _cachedResultVault),
          config, _tradableCurrenciesCache, _marketsCache, _curlHandle),
      _orderBookCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kOrderBook), _cachedResultVault), config,
          _tradableCurrenciesCache, _marketsCache, _curlHandle) {}

ExchangePublic::WithdrawalFeeMap KrakenPublic::WithdrawalFeesFunc::operator()() {
  WithdrawalFeeMap ret;
  json jsonData = OpenJsonFile("withdrawfees.json", FileNotFoundMode::kThrow, FileType::kData);
  for (const auto& [coin, value] : jsonData[_name].items()) {
    CurrencyCode coinAcro(coin);
    MonetaryAmount ma(value.get<std::string_view>(), coinAcro);
    log::debug("Updated Kraken withdrawal fees {}", ma.str());
    ret.insert_or_assign(coinAcro, ma);
  }
  log::info("Updated Kraken withdrawal fees for {} coins", ret.size());
  return ret;
}

CurrencyExchangeFlatSet KrakenPublic::TradableCurrenciesFunc::operator()() {
  json result = PublicQuery(_curlHandle, "Assets");
  CurrencyExchangeFlatSet currencies;
  currencies.reserve(static_cast<CurrencyExchangeFlatSet::size_type>(result.size()));
  const ExchangeInfo::CurrencySet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  for (const auto& [krakenAssetName, value] : result.items()) {
    std::string_view altCodeStr = value["altname"].get<std::string_view>();
    if (!CheckCurrencyExchange(krakenAssetName, altCodeStr, excludedCurrencies, _config)) {
      continue;
    }
    CurrencyCode standardCode(_config.standardizeCurrencyCode(altCodeStr));
    CurrencyExchange newCurrency(standardCode, CurrencyCode(krakenAssetName), CurrencyCode(altCodeStr),
                                 CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable);

    if (currencies.contains(newCurrency)) {
      log::error("Duplicated {}", newCurrency.str());
    } else {
      log::debug("Retrieved Kraken Currency {}", newCurrency.str());
      currencies.insert(std::move(newCurrency));
    }
  }
  log::info("Retrieved {} Kraken currencies", currencies.size());
  return currencies;
}

std::pair<KrakenPublic::MarketSet, KrakenPublic::MarketsFunc::MarketInfoMap> KrakenPublic::MarketsFunc::operator()() {
  json result = PublicQuery(_curlHandle, "AssetPairs");
  /*
    "ADAAUD":{"altname":"ADAAUD","wsname":"ADA/AUD","aclass_base":"currency","base":"ADA",
    "aclass_quote":"currency","quote":"ZAUD","lot":"unit","pair_decimals":5,"lot_decimals":8,
    "lot_multiplier":1,"leverage_buy":[],"leverage_sell":[],
    "fees":[[0,0.26],[50000,0.24],[100000,0.22],[250000,0.2],[500000,0.18],[1000000,0.16],[2500000,0.14],[5000000,0.12],[10000000,0.1]],
    "fees_maker":[[0,0.16],[50000,0.14],[100000,0.12],[250000,0.1],[500000,0.08],[1000000,0.06],[2500000,0.04],[5000000,0.02],[10000000,0]],
    "fee_volume_currency":"ZUSD","margin_call":80,"margin_stop":40,"ordermin":"25"}
  */
  std::pair<MarketSet, MarketInfoMap> ret;
  ret.first.reserve(static_cast<MarketSet::size_type>(result.size()));
  ret.second.reserve(result.size());
  const ExchangeInfo::CurrencySet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  const CurrencyExchangeFlatSet& currencies = _tradableCurrenciesCache.get();
  for (const auto& [key, value] : result.items()) {
    if (!value.contains("ordermin")) {
      log::debug("Discard market {} as it does not contain min order information", key);
      continue;
    }
    std::string_view krakenBaseStr = value["base"].get<std::string_view>();
    CurrencyCode base(_config.standardizeCurrencyCode(krakenBaseStr));
    auto baseIt = currencies.find(base);
    if (baseIt == currencies.end()) {
      continue;
    }
    const CurrencyExchange& baseExchange = *baseIt;
    if (!CheckCurrencyExchange(krakenBaseStr, baseExchange.altStr(), excludedCurrencies, _config)) {
      continue;
    }

    std::string_view krakenQuoteStr = value["quote"].get<std::string_view>();
    CurrencyCode quote(_config.standardizeCurrencyCode(krakenQuoteStr));
    auto quoteIt = currencies.find(quote);
    if (quoteIt == currencies.end()) {
      continue;
    }
    const CurrencyExchange& quoteExchange = *quoteIt;
    if (!CheckCurrencyExchange(krakenQuoteStr, quoteExchange.altStr(), excludedCurrencies, _config)) {
      continue;
    }
    auto mkIt = ret.first.emplace(base, quote).first;
    log::debug("Retrieved Kraken market {}", mkIt->str());
    MonetaryAmount orderMin(value["ordermin"].get<std::string_view>(), base);
    ret.second.insert_or_assign(
        *mkIt, MarketInfo{VolAndPriNbDecimals(value["lot_decimals"], value["pair_decimals"]), orderMin});
  }
  log::info("Retrieved {} markets from Kraken", ret.first.size());
  return ret;
}

ExchangePublic::MarketOrderBookMap KrakenPublic::AllOrderBooksFunc::operator()(int depth) {
  MarketOrderBookMap ret;
  std::string allAssetPairs;
  const CurrencyExchangeFlatSet& krakenCurrencies = _tradableCurrenciesCache.get();
  const MarketSet& markets = _marketsCache.get().first;
  allAssetPairs.reserve(markets.size() * 8);
  using KrakenAssetPairToStdMarketMap = std::unordered_map<std::string, Market>;
  KrakenAssetPairToStdMarketMap krakenAssetPairToStdMarketMap;
  krakenAssetPairToStdMarketMap.reserve(markets.size());
  ret.reserve(markets.size());
  for (Market m : markets) {
    auto lb = krakenCurrencies.find(m.base());
    if (lb == krakenCurrencies.end()) {
      throw exception("Cannot find " + std::string(m.base().str()) + " in Kraken currencies");
    }
    CurrencyExchange krakenCurrencyExchangeBase = *lb;
    lb = krakenCurrencies.find(m.quote());
    if (lb == krakenCurrencies.end()) {
      throw exception("Cannot find " + std::string(m.quote().str()) + " in Kraken currencies");
    }
    CurrencyExchange krakenCurrencyExchangeQuote = *lb;
    Market krakenMarket(krakenCurrencyExchangeBase.altStr(), krakenCurrencyExchangeQuote.altStr());
    std::string assetPairStr = krakenMarket.assetsPairStr();
    if (!allAssetPairs.empty()) {
      allAssetPairs.push_back(',');
    }
    allAssetPairs.append(assetPairStr);
    krakenAssetPairToStdMarketMap.insert_or_assign(assetPairStr, m);
    krakenAssetPairToStdMarketMap.insert_or_assign(
        Market(krakenCurrencyExchangeBase.exchangeCode(), krakenCurrencyExchangeQuote.exchangeCode()).assetsPairStr(),
        m);
  }
  json result = PublicQuery(_curlHandle, "Ticker", {{"pair", allAssetPairs}});
  for (const auto& [krakenAssetPair, assetPairDetails] : result.items()) {
    if (krakenAssetPairToStdMarketMap.find(krakenAssetPair) == krakenAssetPairToStdMarketMap.end()) {
      log::error("Unable to find {}", krakenAssetPair);
      continue;
    }

    Market m = krakenAssetPairToStdMarketMap.find(krakenAssetPair)->second;
    m = Market(CurrencyCode(_config.standardizeCurrencyCode(std::string(m.base().str()))),
               CurrencyCode(_config.standardizeCurrencyCode(std::string(m.quote().str()))));
    //  a = ask array(<price>, <whole lot volume>, <lot volume>)
    //  b = bid array(<price>, <whole lot volume>, <lot volume>)
    const json& askDetails = assetPairDetails["a"];
    const json& bidDetails = assetPairDetails["b"];
    MonetaryAmount askPri(askDetails[0].get<std::string_view>(), m.quote());
    MonetaryAmount bidPri(bidDetails[0].get<std::string_view>(), m.quote());
    MonetaryAmount askVol(askDetails[2].get<std::string_view>(), m.base());
    MonetaryAmount bidVol(bidDetails[2].get<std::string_view>(), m.base());

    ret.insert_or_assign(m, MarketOrderBook(askPri, askVol, bidPri, bidVol, depth));
  }

  log::info("Retrieved ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook KrakenPublic::OrderBookFunc::operator()(Market m, int count) {
  CurrencyExchangeFlatSet krakenCurrencies = _tradableCurrenciesCache.get();
  auto lb = krakenCurrencies.find(m.base());
  if (lb == krakenCurrencies.end()) {
    throw exception("Cannot find " + std::string(m.base().str()) + " in Kraken currencies");
  }
  CurrencyExchange krakenCurrencyExchangeBase = *lb;
  lb = krakenCurrencies.find(m.quote());
  if (lb == krakenCurrencies.end()) {
    throw exception("Cannot find " + std::string(m.quote().str()) + " in Kraken currencies");
  }
  CurrencyExchange krakenCurrencyExchangeQuote = *lb;
  std::string krakenAssetPair(krakenCurrencyExchangeBase.altStr());
  krakenAssetPair.append(krakenCurrencyExchangeQuote.altStr());
  json result = PublicQuery(_curlHandle, "Depth", {{"pair", krakenAssetPair}, {"count", std::to_string(count)}});
  const json& entry = result.front();
  const json& asks = entry["asks"];
  const json& bids = entry["bids"];

  auto volAndPriNbDecimals = _marketsCache.get().second.find(m)->second.volAndPriNbDecimals;
  using OrderBookVec = cct::vector<OrderBookLine>;
  OrderBookVec orderBookLines;
  orderBookLines.reserve(static_cast<OrderBookVec::size_type>(asks.size() + bids.size()));
  for (auto asksOrBids : {std::addressof(asks), std::addressof(bids)}) {
    const bool isAsk = asksOrBids == std::addressof(asks);
    for (const auto& priceQuantityTuple : *asksOrBids) {
      std::string priceStr = priceQuantityTuple[0];
      std::string amountStr = priceQuantityTuple[1];

      MonetaryAmount amount(amountStr, m.base());
      MonetaryAmount price(priceStr, m.quote());

      orderBookLines.emplace_back(amount, price, isAsk);
    }
  }
  return MarketOrderBook(m, orderBookLines, volAndPriNbDecimals);
}

}  // namespace api
}  // namespace cct
