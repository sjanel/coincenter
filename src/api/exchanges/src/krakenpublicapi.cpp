#include "krakenpublicapi.hpp"

#include <cassert>
#include <fstream>
#include <unordered_map>

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"

namespace cct {
namespace api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view method, CurlPostData&& postData = CurlPostData()) {
  string method_url = KrakenPublic::kUrlBase;
  method_url.push_back('/');
  method_url.push_back(KrakenPublic::kVersion);
  method_url.append("/public/");
  method_url.append(method);

  CurlOptions opts(CurlOptions::RequestType::kGet, std::move(postData), KrakenPublic::kUserAgent);
  string ret = curlHandle.query(method_url, opts);
  json jsonData = json::parse(std::move(ret));
  if (jsonData.contains("error") && !jsonData["error"].empty()) {
    throw exception("Kraken public query error: " + string(jsonData["error"].front()));
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

File GetKrakenWithdrawInfoFile(std::string_view dataDir) {
  return File(dataDir, File::Type::kCache, "krakenwithdrawinfo.json", File::IfNotFound::kNoThrow);
}

}  // namespace

KrakenPublic::KrakenPublic(CoincenterInfo& config, FiatConverter& fiatConverter, CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("kraken", fiatConverter, cryptowatchAPI, config),
      _curlHandle(config.metricGatewayPtr(), config.exchangeInfo(_name).minPublicQueryDelay(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kCurrencies), _cachedResultVault), config,
          config.exchangeInfo(_name), _curlHandle),
      _withdrawalFeesCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kWithdrawalFees), _cachedResultVault),
          config),
      _marketsCache(CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kMarkets), _cachedResultVault),
                    config, _tradableCurrenciesCache, _curlHandle, config.exchangeInfo(_name)),
      _allOrderBooksCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kAllOrderBooks), _cachedResultVault),
          config, _tradableCurrenciesCache, _marketsCache, _curlHandle),
      _orderBookCache(
          CachedResultOptions(config.getAPICallUpdateFrequency(QueryTypeEnum::kOrderBook), _cachedResultVault), config,
          _tradableCurrenciesCache, _marketsCache, _curlHandle),
      _tickerCache(CachedResultOptions(std::min(config.getAPICallUpdateFrequency(QueryTypeEnum::kTradedVolume),
                                                config.getAPICallUpdateFrequency(QueryTypeEnum::kLastPrice)),
                                       _cachedResultVault),
                   _tradableCurrenciesCache, _curlHandle) {
  // To save queries to Kraken site, let's check if there is recent cached data
  json data = GetKrakenWithdrawInfoFile(_coincenterInfo.dataDir()).readJson();
  if (!data.empty()) {
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = Clock::duration;

    Duration withdrawDataRefreshTime = config.getAPICallUpdateFrequency(QueryTypeEnum::kWithdrawalFees);
    TimePoint lastUpdatedTime(std::chrono::seconds(data["timeepoch"].get<int64_t>()));
    if (Clock::now() < lastUpdatedTime + withdrawDataRefreshTime) {
      // we can reuse file data
      KrakenPublic::WithdrawalFeesFunc::WithdrawalInfoMaps withdrawalInfoMaps;

      for (const auto& [curCodeStr, val] : data["assets"].items()) {
        CurrencyCode cur(curCodeStr);
        MonetaryAmount withdrawMin(val["min"].get<std::string_view>(), cur);
        MonetaryAmount withdrawFee(val["fee"].get<std::string_view>(), cur);

        log::trace("Updated {} withdrawal fee {} from cache", _name, withdrawFee.str());
        log::trace("Updated {} min withdraw {} from cache", _name, withdrawMin.str());

        withdrawalInfoMaps.first.insert_or_assign(cur, withdrawFee);
        withdrawalInfoMaps.second.insert_or_assign(cur, withdrawMin);
      }

      _withdrawalFeesCache.set(std::move(withdrawalInfoMaps), lastUpdatedTime);
    }
  }
}

KrakenPublic::WithdrawalFeesFunc::WithdrawalInfoMaps KrakenPublic::WithdrawalFeesFunc::operator()() {
  // Retrieve public file from Google Drive - Method found in
  // https://gist.github.com/tanaikech/f0f2d122e05bf5f971611258c22c110f
  constexpr char kWithdrawalFeesCSVUrl[] =
      "https://drive.google.com/uc?export=download&id=1tkvmX25d3uV_SWS2NEyfRvrSO2t1P3PJ";

  CurlOptions curlOptions(CurlOptions::RequestType::kGet);
  curlOptions.followLocation = true;
  string withdrawalFeesCsv = _curlHandle.query(kWithdrawalFeesCSVUrl, curlOptions);

  if (withdrawalFeesCsv.empty()) {
    log::warn("Kraken withdrawal fees CSV file cannot be retrieved dynamically. URL has maybe changed?");
    log::warn("Defaulted to hardcoded provided CSV file");
    withdrawalFeesCsv =
        File(_config.dataDir(), File::Type::kCache, "krakenwithdrawalfees.csv", File::IfNotFound::kThrow).read();
  }

  std::size_t assetPos = std::string_view::npos;
  std::size_t minWithdrawAmountPos = std::string_view::npos;
  std::size_t withdrawFeePos = std::string_view::npos;

  bool isFirstLine = true;

  // Let's parse this CSV manually (it's not so difficult)
  WithdrawalInfoMaps ret;
  for (std::size_t nextLinePos = withdrawalFeesCsv.find_first_of('\n'), lineBegPos = 0;;
       nextLinePos = withdrawalFeesCsv.find_first_of('\n', lineBegPos)) {
    std::string_view line(std::next(withdrawalFeesCsv.begin(), lineBegPos),
                          nextLinePos == std::string_view::npos ? withdrawalFeesCsv.end()
                                                                : std::next(withdrawalFeesCsv.begin(), nextLinePos));
    if (line.empty()) {
      break;
    }
    if (line.back() == 13) {  // Windows style 'CR' end of line to be removed
      line.remove_suffix(1);
    }

    CurrencyCode cur;
    std::string_view withdrawFeeMinStr[2];
    for (std::size_t commaPos = line.find_first_of(','), fieldBegPos = 0, fieldPos = 0;;
         commaPos = line.find_first_of(',', fieldBegPos), ++fieldPos) {
      std::string_view field(std::next(line.begin(), fieldBegPos),
                             commaPos == std::string_view::npos ? line.end() : std::next(line.begin(), commaPos));

      // Lines should look like this:
      //  USDT,5,2.5
      //  USDT (ERC20),5,2.5
      //  USDT (OMNI),10,5
      //  USDT (TRC20),2,1
      // In case several variations of coins are present, just take the max for all as a defensive assumption.

      if (isFirstLine) {
        if (field == "Asset") {
          assetPos = fieldPos;
        } else if (field == "Minimum") {
          minWithdrawAmountPos = fieldPos;
        } else if (field == "Fee") {
          withdrawFeePos = fieldPos;
        }
      } else {
        if (fieldPos == assetPos) {
          std::size_t spacePos = field.find_first_of(" (");
          if (spacePos != std::string_view::npos) {
            field = field.substr(0, spacePos);
          }
          cur = CurrencyCode(_config.standardizeCurrencyCode(field));
        } else if (fieldPos == withdrawFeePos) {
          withdrawFeeMinStr[0] = field;
        } else if (fieldPos == minWithdrawAmountPos) {
          withdrawFeeMinStr[1] = field;
        }
      }

      if (commaPos == std::string_view::npos) {
        break;
      }
      fieldBegPos = commaPos + 1;
    }

    if (isFirstLine) {
      isFirstLine = false;
      if (assetPos == std::string_view::npos || minWithdrawAmountPos == std::string_view::npos ||
          withdrawFeePos == std::string_view::npos) {
        throw exception("Unable to parse Kraken withdrawal fees CSV, syntax has probably changed");
      }
    } else {
      bool isFeeMap = true;
      for (std::string_view wStr : withdrawFeeMinStr) {
        if (wStr != "*") {
          MonetaryAmount amount(wStr, cur);
          auto& map = isFeeMap ? ret.first : ret.second;
          auto it = map.find(cur);
          if (it == map.end() || it->second < amount) {
            log::trace("Updated Kraken {} {}", isFeeMap ? "withdrawal fee" : "min withdraw", amount.str());
            if (it == map.end()) {
              map.insert_or_assign(cur, amount);
            } else {
              it->second = amount;
            }
          }
        }
        isFeeMap = false;
      }
    }

    if (nextLinePos == std::string_view::npos) {
      break;
    }
    lineBegPos = nextLinePos + 1;
  }
  if (assetPos == std::string_view::npos || minWithdrawAmountPos == std::string_view::npos ||
      withdrawFeePos == std::string_view::npos) {
    throw exception("Unable to parse Kraken withdrawal fees CSV, syntax or URL has probably changed");
  }

  log::info("Updated Kraken withdraw infos for {} coins", ret.first.size());
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
  string allAssetPairs;
  const CurrencyExchangeFlatSet& krakenCurrencies = _tradableCurrenciesCache.get();
  const auto& [markets, marketInfoMap] = _marketsCache.get();
  allAssetPairs.reserve(markets.size() * 8);
  using KrakenAssetPairToStdMarketMap = std::unordered_map<string, Market>;
  KrakenAssetPairToStdMarketMap krakenAssetPairToStdMarketMap;
  krakenAssetPairToStdMarketMap.reserve(markets.size());
  ret.reserve(markets.size());
  for (Market m : markets) {
    auto lb = krakenCurrencies.find(m.base());
    if (lb == krakenCurrencies.end()) {
      throw exception("Cannot find " + string(m.base().str()) + " in Kraken currencies");
    }
    CurrencyExchange krakenCurrencyExchangeBase = *lb;
    lb = krakenCurrencies.find(m.quote());
    if (lb == krakenCurrencies.end()) {
      throw exception("Cannot find " + string(m.quote().str()) + " in Kraken currencies");
    }
    CurrencyExchange krakenCurrencyExchangeQuote = *lb;
    Market krakenMarket(krakenCurrencyExchangeBase.altStr(), krakenCurrencyExchangeQuote.altStr());
    string assetPairStr = krakenMarket.assetsPairStr();
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
    m = Market(CurrencyCode(_config.standardizeCurrencyCode(m.base().str())),
               CurrencyCode(_config.standardizeCurrencyCode(m.quote().str())));
    //  a = ask array(<price>, <whole lot volume>, <lot volume>)
    //  b = bid array(<price>, <whole lot volume>, <lot volume>)
    const json& askDetails = assetPairDetails["a"];
    const json& bidDetails = assetPairDetails["b"];
    MonetaryAmount askPri(askDetails[0].get<std::string_view>(), m.quote());
    MonetaryAmount bidPri(bidDetails[0].get<std::string_view>(), m.quote());
    MonetaryAmount askVol(askDetails[2].get<std::string_view>(), m.base());
    MonetaryAmount bidVol(bidDetails[2].get<std::string_view>(), m.base());

    const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(m)->second;

    ret.insert_or_assign(m, MarketOrderBook(askPri, askVol, bidPri, bidVol, marketInfo.volAndPriNbDecimals, depth));
  }

  log::info("Retrieved ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook KrakenPublic::OrderBookFunc::operator()(Market m, int count) {
  CurrencyExchangeFlatSet krakenCurrencies = _tradableCurrenciesCache.get();
  auto lb = krakenCurrencies.find(m.base());
  if (lb == krakenCurrencies.end()) {
    throw exception("Cannot find " + string(m.base().str()) + " in Kraken currencies");
  }
  CurrencyExchange krakenCurrencyExchangeBase = *lb;
  lb = krakenCurrencies.find(m.quote());
  if (lb == krakenCurrencies.end()) {
    throw exception("Cannot find " + string(m.quote().str()) + " in Kraken currencies");
  }
  CurrencyExchange krakenCurrencyExchangeQuote = *lb;
  string krakenAssetPair(krakenCurrencyExchangeBase.altStr());
  krakenAssetPair.append(krakenCurrencyExchangeQuote.altStr());
  json result = PublicQuery(_curlHandle, "Depth", {{"pair", krakenAssetPair}, {"count", count}});
  const json& entry = result.front();
  const json& asks = entry["asks"];
  const json& bids = entry["bids"];

  auto volAndPriNbDecimals = _marketsCache.get().second.find(m)->second.volAndPriNbDecimals;
  using OrderBookVec = vector<OrderBookLine>;
  OrderBookVec orderBookLines;
  orderBookLines.reserve(static_cast<OrderBookVec::size_type>(asks.size() + bids.size()));
  for (auto asksOrBids : {std::addressof(asks), std::addressof(bids)}) {
    const bool isAsk = asksOrBids == std::addressof(asks);
    for (const auto& priceQuantityTuple : *asksOrBids) {
      std::string_view priceStr = priceQuantityTuple[0].get<std::string_view>();
      std::string_view amountStr = priceQuantityTuple[1].get<std::string_view>();

      MonetaryAmount amount(amountStr, m.base());
      MonetaryAmount price(priceStr, m.quote());

      orderBookLines.emplace_back(amount, price, isAsk);
    }
  }
  return MarketOrderBook(m, orderBookLines, volAndPriNbDecimals);
}

KrakenPublic::TickerFunc::Last24hTradedVolumeAndLatestPricePair KrakenPublic::TickerFunc::operator()(Market m) {
  Market krakenMarket(_tradableCurrenciesCache.get().getOrThrow(m.base()).altStr(),
                      _tradableCurrenciesCache.get().getOrThrow(m.quote()).altStr());
  json result = PublicQuery(_curlHandle, "Ticker", {{"pair", krakenMarket.assetsPairStr()}});
  for (const auto& [krakenAssetPair, assetPairDetails] : result.items()) {
    std::string_view last24hVol = assetPairDetails["v"][1].get<std::string_view>();
    std::string_view lastTickerPrice = assetPairDetails["c"][0].get<std::string_view>();
    return {MonetaryAmount(last24hVol, m.base()), MonetaryAmount(lastTickerPrice, m.quote())};
  }
  throw exception("Invalid data retrieved from ticker information");
}

void KrakenPublic::updateCacheFile() const {
  const auto [withdrawalInfoMapsPtr, latestUpdate] = _withdrawalFeesCache.retrieve();
  if (withdrawalInfoMapsPtr) {
    using WithdrawalInfoMaps = KrakenPublic::WithdrawalFeesFunc::WithdrawalInfoMaps;

    const WithdrawalInfoMaps& withdrawalInfoMaps = *withdrawalInfoMapsPtr;

    json data;
    data["timeepoch"] = std::chrono::duration_cast<std::chrono::seconds>(latestUpdate.time_since_epoch()).count();
    for (const auto& [curCode, withdrawFee] : withdrawalInfoMaps.first) {
      string curCodeStr(curCode.str());
      data["assets"][curCodeStr]["min"] = withdrawalInfoMaps.second.find(curCode)->second.amountStr();
      data["assets"][curCodeStr]["fee"] = withdrawFee.amountStr();
    }
    GetKrakenWithdrawInfoFile(_coincenterInfo.dataDir()).write(data);
  }
}

}  // namespace api
}  // namespace cct
