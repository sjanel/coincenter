#include "krakenpublicapi.hpp"

#include <cassert>
#include <fstream>
#include <unordered_map>

#include "cct_cctype.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "timedef.hpp"

namespace cct::api {
namespace {

json PublicQuery(CurlHandle& curlHandle, std::string_view method, CurlPostData&& postData = CurlPostData()) {
  json ret = json::parse(
      curlHandle.query(method, CurlOptions(HttpRequestType::kGet, std::move(postData), KrakenPublic::kUserAgent)));
  auto errorIt = ret.find("error");
  if (errorIt != ret.end() && !errorIt->empty()) {
    log::error("Full Kraken json error: '{}'", ret.dump());
    throw exception("Kraken error: {}", errorIt->front().get<std::string_view>());
  }
  return ret["result"];
}

bool CheckCurrencyExchange(std::string_view krakenEntryCurrencyCode, std::string_view krakenAltName,
                           const CurrencyCodeSet& excludedCurrencies, const CoincenterInfo& config) {
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
  static constexpr bool kAvoidAugurV1AndKeepAugurV2 = true;

  if constexpr (kAvoidAugurV1AndKeepAugurV2) {
    if (krakenEntryCurrencyCode == "XREP") {
      log::trace("Discard {} favored by Augur V2", krakenEntryCurrencyCode);
      return false;
    }
  }

  CurrencyCode standardCode(config.standardizeCurrencyCode(krakenAltName));
  if (excludedCurrencies.contains(standardCode)) {
    // Forbidden currency, do not consider its market
    log::trace("Discard {} excluded by config", standardCode);
    return false;
  }
  return true;
}

File GetKrakenWithdrawInfoFile(std::string_view dataDir) {
  return File(dataDir, File::Type::kCache, "krakenwithdrawinfo.json", File::IfError::kNoThrow);
}

constexpr std::string_view kUrlWithdrawFee1 = "https://withdrawalfees.com/exchanges/kraken";
constexpr std::string_view kUrlWithdrawFee2 = "https://www.cryptofeesaver.com/exchanges/fees/kraken";

}  // namespace

KrakenPublic::WithdrawalFeesFunc::WithdrawalFeesFunc(const CoincenterInfo& coincenterInfo,
                                                     Duration minDurationBetweenQueries)
    : _curlHandle1(kUrlWithdrawFee1, coincenterInfo.metricGatewayPtr(), minDurationBetweenQueries,
                   coincenterInfo.getRunMode()),
      _curlHandle2(kUrlWithdrawFee2, coincenterInfo.metricGatewayPtr(), minDurationBetweenQueries,
                   coincenterInfo.getRunMode()) {}

KrakenPublic::KrakenPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CryptowatchAPI& cryptowatchAPI)
    : ExchangePublic("kraken", fiatConverter, cryptowatchAPI, config),
      _curlHandle(kUrlBase, config.metricGatewayPtr(), exchangeInfo().publicAPIRate(), config.getRunMode()),
      _tradableCurrenciesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kCurrencies), _cachedResultVault), config,
          cryptowatchAPI, _curlHandle, exchangeInfo()),
      _withdrawalFeesCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kWithdrawalFees), _cachedResultVault), config,
          exchangeInfo().publicAPIRate()),
      _marketsCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kMarkets), _cachedResultVault),
                    _tradableCurrenciesCache, config, _curlHandle, exchangeInfo()),
      _allOrderBooksCache(
          CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kAllOrderBooks), _cachedResultVault),
          _tradableCurrenciesCache, _marketsCache, config, _curlHandle),
      _orderBookCache(CachedResultOptions(exchangeInfo().getAPICallUpdateFrequency(kOrderBook), _cachedResultVault),
                      _tradableCurrenciesCache, _marketsCache, _curlHandle),
      _tickerCache(CachedResultOptions(std::min(exchangeInfo().getAPICallUpdateFrequency(kTradedVolume),
                                                exchangeInfo().getAPICallUpdateFrequency(kLastPrice)),
                                       _cachedResultVault),
                   _tradableCurrenciesCache, _curlHandle) {
  json data = GetKrakenWithdrawInfoFile(_coincenterInfo.dataDir()).readJson();
  if (!data.empty()) {
    Duration withdrawDataRefreshTime = exchangeInfo().getAPICallUpdateFrequency(kWithdrawalFees);
    TimePoint lastUpdatedTime(std::chrono::seconds(data["timeepoch"].get<int64_t>()));
    if (Clock::now() < lastUpdatedTime + withdrawDataRefreshTime) {
      // we can reuse file data
      KrakenPublic::WithdrawalFeesFunc::WithdrawalInfoMaps withdrawalInfoMaps;

      for (const auto& [curCodeStr, val] : data["assets"].items()) {
        CurrencyCode cur(curCodeStr);
        MonetaryAmount withdrawMin(val["min"].get<std::string_view>(), cur);
        MonetaryAmount withdrawFee(val["fee"].get<std::string_view>(), cur);

        log::trace("Updated {} withdrawal fee {} from cache", _name, withdrawFee);
        log::trace("Updated {} min withdraw {} from cache", _name, withdrawMin);

        withdrawalInfoMaps.first.insert_or_assign(cur, withdrawFee);
        withdrawalInfoMaps.second.insert_or_assign(cur, withdrawMin);
      }

      _withdrawalFeesCache.set(std::move(withdrawalInfoMaps), lastUpdatedTime);
    }
  }
}

MonetaryAmount KrakenPublic::queryWithdrawalFee(CurrencyCode currencyCode) {
  const WithdrawalFeeMap& withdrawalFeeMaps = _withdrawalFeesCache.get().first;
  auto foundIt = withdrawalFeeMaps.find(currencyCode);
  if (foundIt == withdrawalFeeMaps.end()) {
    log::error("Unable to find {} withdrawal fee for {}", name(), currencyCode);
    return MonetaryAmount(0, currencyCode);
  }
  return foundIt->second;
}

namespace {
constexpr std::string_view kParseError1Msg =
    "Parse error from source 1 - either site information unavailable or code to be updated";
}

KrakenPublic::WithdrawalFeesFunc::WithdrawalInfoMaps KrakenPublic::WithdrawalFeesFunc::updateFromSource1() {
  string withdrawalFeesCsv = _curlHandle1.query("", CurlOptions(HttpRequestType::kGet));

  static constexpr std::string_view kBeginWithdrawalFeeHtmlTag = "<td class=withdrawalFee>";
  static constexpr std::string_view kBeginMinWithdrawalHtmlTag = "<td class=minWithdrawal>";

  WithdrawalInfoMaps ret;

  std::size_t searchPos = 0;
  while ((searchPos = withdrawalFeesCsv.find(kBeginWithdrawalFeeHtmlTag, searchPos)) != string::npos) {
    auto parseNextFee = [&withdrawalFeesCsv](std::size_t& begPos) {
      static constexpr std::string_view kBeginFeeHtmlTag = "<div class=fee>";
      static constexpr std::string_view kEndHtmlTag = "</div>";

      MonetaryAmount ret;

      begPos = withdrawalFeesCsv.find(kBeginFeeHtmlTag, begPos);
      if (begPos == string::npos) {
        log::error(kParseError1Msg);
        return ret;
      }
      begPos += kBeginFeeHtmlTag.size();
      // There are sometimes strange characters at beginning of the amount
      while (!isdigit(withdrawalFeesCsv[begPos])) {
        ++begPos;
      }
      std::size_t endPos = withdrawalFeesCsv.find(kEndHtmlTag, begPos + 1);
      if (endPos == string::npos) {
        log::error(kParseError1Msg);
        return ret;
      }
      ret = MonetaryAmount(std::string_view(withdrawalFeesCsv.begin() + begPos, withdrawalFeesCsv.begin() + endPos));
      begPos = endPos + kEndHtmlTag.size();
      return ret;
    };

    // Locate withdrawal fee
    searchPos += kBeginWithdrawalFeeHtmlTag.size();
    MonetaryAmount withdrawalFee = parseNextFee(searchPos);
    if (withdrawalFee.currencyCode().isNeutral()) {
      ret.first.clear();
      break;
    }

    log::trace("Updated Kraken withdrawal fee {} from first source", withdrawalFee);
    ret.first.insert_or_assign(withdrawalFee.currencyCode(), withdrawalFee);

    // Locate min withdrawal
    searchPos = withdrawalFeesCsv.find(kBeginMinWithdrawalHtmlTag, searchPos) + kBeginMinWithdrawalHtmlTag.size();
    if (searchPos == string::npos) {
      log::error(kParseError1Msg);
      ret.first.clear();
      break;
    }

    MonetaryAmount minWithdrawal = parseNextFee(searchPos);
    if (minWithdrawal.currencyCode().isNeutral()) {
      ret.first.clear();
      break;
    }

    log::trace("Updated Kraken min withdrawal {} from first source", minWithdrawal);
    ret.second.insert_or_assign(minWithdrawal.currencyCode(), minWithdrawal);
  }
  if (ret.first.empty() || ret.second.empty()) {
    log::error("Unable to parse Kraken withdrawal fees from first source");
  } else {
    log::info("Updated Kraken withdraw infos for {} coins from first source", ret.first.size());
  }
  return ret;
}

KrakenPublic::WithdrawalFeesFunc::WithdrawalInfoMaps KrakenPublic::WithdrawalFeesFunc::updateFromSource2() {
  string withdrawalFeesCsv = _curlHandle2.query("", CurlOptions(HttpRequestType::kGet));

  static constexpr std::string_view kBeginTableTitle = "Kraken Deposit & Withdrawal fees</h2>";

  std::size_t begPos = withdrawalFeesCsv.find(kBeginTableTitle);
  WithdrawalInfoMaps ret;
  if (begPos != string::npos) {
    static constexpr std::string_view kBeginTable = "<table class=";
    begPos = withdrawalFeesCsv.find(kBeginTable, begPos + kBeginTableTitle.size());
    if (begPos != string::npos) {
      static constexpr std::string_view kBeginWithdrawalFeeHtmlTag = R"(<th scope="row" class="align)";

      std::size_t searchPos = begPos + kBeginTable.size();
      while ((searchPos = withdrawalFeesCsv.find(kBeginWithdrawalFeeHtmlTag, searchPos)) != string::npos) {
        auto parseNextFee = [&withdrawalFeesCsv](std::size_t& begPos) {
          static constexpr std::string_view kBeginFeeHtmlTag = "<td class=\"align-middle align-right\">";
          static constexpr std::string_view kEndHtmlTag = "</td>";

          // Skip one column
          for (int colPos = 0; colPos < 2; ++colPos) {
            begPos = withdrawalFeesCsv.find(kBeginFeeHtmlTag, begPos);
            assert(begPos != string::npos);
            begPos += kBeginFeeHtmlTag.size();
          }
          // Scan until next non space char
          while (begPos < withdrawalFeesCsv.size() && isspace(withdrawalFeesCsv[begPos])) {
            ++begPos;
          }
          std::size_t endPos = withdrawalFeesCsv.find(kEndHtmlTag, begPos + 1);
          std::size_t endHtmlTagPos = endPos;
          assert(endPos != string::npos);
          while (endPos > begPos && isspace(withdrawalFeesCsv[endPos - 1])) {
            --endPos;
          }
          MonetaryAmount ret(std::string_view(withdrawalFeesCsv.begin() + begPos, withdrawalFeesCsv.begin() + endPos));
          begPos = endHtmlTagPos + kEndHtmlTag.size();
          return ret;
        };

        // Locate withdrawal fee
        searchPos += kBeginWithdrawalFeeHtmlTag.size();
        MonetaryAmount withdrawalFee = parseNextFee(searchPos);

        log::trace("Updated Kraken withdrawal fee {} from source 2, simulate min withdrawal amount", withdrawalFee);
        ret.first.insert_or_assign(withdrawalFee.currencyCode(), withdrawalFee);

        ret.second.insert_or_assign(withdrawalFee.currencyCode(), 3 * withdrawalFee);
      }
    }
  }

  if (ret.first.empty() || ret.second.empty()) {
    log::error("Unable to parse Kraken withdrawal fees from second source");
  } else {
    log::info("Updated Kraken withdraw infos for {} coins from second source", ret.first.size());
  }
  return ret;
}

KrakenPublic::WithdrawalFeesFunc::WithdrawalInfoMaps KrakenPublic::WithdrawalFeesFunc::operator()() {
  WithdrawalInfoMaps ret = updateFromSource1();
  if (ret.first.empty()) {
    ret = updateFromSource2();
  }
  if (ret.first.empty() || ret.second.empty()) {
    throw exception("Unable to parse Kraken withdrawal fees");
  }
  return ret;
}

CurrencyExchangeFlatSet KrakenPublic::TradableCurrenciesFunc::operator()() {
  json result = PublicQuery(_curlHandle, "/public/Assets");
  CurrencyExchangeVector currencies;
  const CurrencyCodeSet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  for (const auto& [krakenAssetName, value] : result.items()) {
    std::string_view altCodeStr = value["altname"].get<std::string_view>();
    if (!CheckCurrencyExchange(krakenAssetName, altCodeStr, excludedCurrencies, _coincenterInfo)) {
      continue;
    }
    CurrencyCode standardCode(_coincenterInfo.standardizeCurrencyCode(altCodeStr));
    CurrencyExchange newCurrency(standardCode, CurrencyCode(krakenAssetName), CurrencyCode(altCodeStr),
                                 CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                                 _cryptowatchApi.queryIsCurrencyCodeFiat(standardCode)
                                     ? CurrencyExchange::Type::kFiat
                                     : CurrencyExchange::Type::kCrypto);

    log::debug("Retrieved Kraken Currency {}", newCurrency.str());
    currencies.push_back(std::move(newCurrency));
  }
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} Kraken currencies", ret.size());
  return ret;
}

std::pair<MarketSet, KrakenPublic::MarketsFunc::MarketInfoMap> KrakenPublic::MarketsFunc::operator()() {
  json result = PublicQuery(_curlHandle, "/public/AssetPairs");
  std::pair<MarketSet, MarketInfoMap> ret;
  ret.first.reserve(static_cast<MarketSet::size_type>(result.size()));
  ret.second.reserve(result.size());
  const CurrencyCodeSet& excludedCurrencies = _exchangeInfo.excludedCurrenciesAll();
  const CurrencyExchangeFlatSet& currencies = _tradableCurrenciesCache.get();
  for (const auto& [key, value] : result.items()) {
    if (!value.contains("ordermin")) {
      log::debug("Discard market {} as it does not contain min order information", key);
      continue;
    }
    std::string_view krakenBaseStr = value["base"].get<std::string_view>();
    CurrencyCode base(_coincenterInfo.standardizeCurrencyCode(krakenBaseStr));
    auto baseIt = currencies.find(base);
    if (baseIt == currencies.end()) {
      continue;
    }
    const CurrencyExchange& baseExchange = *baseIt;
    if (!CheckCurrencyExchange(krakenBaseStr, baseExchange.altStr(), excludedCurrencies, _coincenterInfo)) {
      continue;
    }

    std::string_view krakenQuoteStr = value["quote"].get<std::string_view>();
    CurrencyCode quote(_coincenterInfo.standardizeCurrencyCode(krakenQuoteStr));
    auto quoteIt = currencies.find(quote);
    if (quoteIt == currencies.end()) {
      continue;
    }
    const CurrencyExchange& quoteExchange = *quoteIt;
    if (!CheckCurrencyExchange(krakenQuoteStr, quoteExchange.altStr(), excludedCurrencies, _coincenterInfo)) {
      continue;
    }
    auto mkIt = ret.first.emplace(base, quote).first;
    log::debug("Retrieved Kraken market {}", *mkIt);
    MonetaryAmount orderMin(value["ordermin"].get<std::string_view>(), base);
    ret.second.insert_or_assign(
        *mkIt, MarketInfo{VolAndPriNbDecimals(value["lot_decimals"], value["pair_decimals"]), orderMin});
  }
  log::info("Retrieved {} markets from Kraken", ret.first.size());
  return ret;
}

MarketOrderBookMap KrakenPublic::AllOrderBooksFunc::operator()(int depth) {
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
      throw exception("Cannot find {} in Kraken currencies", m.base());
    }
    CurrencyExchange krakenCurrencyExchangeBase = *lb;
    lb = krakenCurrencies.find(m.quote());
    if (lb == krakenCurrencies.end()) {
      throw exception("Cannot find {} in Kraken currencies", m.quote());
    }
    CurrencyExchange krakenCurrencyExchangeQuote = *lb;
    Market krakenMarket(krakenCurrencyExchangeBase.altCode(), krakenCurrencyExchangeQuote.altCode());
    string assetPairStr = krakenMarket.assetsPairStrUpper();
    if (!allAssetPairs.empty()) {
      allAssetPairs.push_back(',');
    }
    allAssetPairs.append(assetPairStr);
    krakenAssetPairToStdMarketMap.insert_or_assign(assetPairStr, m);
    krakenAssetPairToStdMarketMap.insert_or_assign(
        Market(krakenCurrencyExchangeBase.exchangeCode(), krakenCurrencyExchangeQuote.exchangeCode())
            .assetsPairStrUpper(),
        m);
  }
  json result = PublicQuery(_curlHandle, "/public/Ticker", {{"pair", allAssetPairs}});
  for (const auto& [krakenAssetPair, assetPairDetails] : result.items()) {
    if (krakenAssetPairToStdMarketMap.find(krakenAssetPair) == krakenAssetPairToStdMarketMap.end()) {
      log::error("Unable to find {}", krakenAssetPair);
      continue;
    }

    Market m = krakenAssetPairToStdMarketMap.find(krakenAssetPair)->second;
    m = Market(CurrencyCode(_coincenterInfo.standardizeCurrencyCode(m.base())),
               CurrencyCode(_coincenterInfo.standardizeCurrencyCode(m.quote())));
    //  a = ask array(<price>, <whole lot volume>, <lot volume>)
    //  b = bid array(<price>, <whole lot volume>, <lot volume>)
    const json& askDetails = assetPairDetails["a"];
    const json& bidDetails = assetPairDetails["b"];
    MonetaryAmount askPri(askDetails[0].get<std::string_view>(), m.quote());
    MonetaryAmount bidPri(bidDetails[0].get<std::string_view>(), m.quote());
    MonetaryAmount askVol(askDetails[2].get<std::string_view>(), m.base());
    MonetaryAmount bidVol(bidDetails[2].get<std::string_view>(), m.base());

    const MarketsFunc::MarketInfo& marketInfo = marketInfoMap.find(m)->second;

    if (bidVol != 0 && askVol != 0) {
      ret.insert_or_assign(m, MarketOrderBook(askPri, askVol, bidPri, bidVol, marketInfo.volAndPriNbDecimals, depth));
    }
  }

  log::info("Retrieved ticker information from {} markets", ret.size());
  return ret;
}

MarketOrderBook KrakenPublic::OrderBookFunc::operator()(Market m, int count) {
  CurrencyExchangeFlatSet krakenCurrencies = _tradableCurrenciesCache.get();
  auto lb = krakenCurrencies.find(m.base());
  if (lb == krakenCurrencies.end()) {
    throw exception("Cannot find {} in Kraken currencies", m.base());
  }
  CurrencyExchange krakenCurrencyExchangeBase = *lb;
  lb = krakenCurrencies.find(m.quote());
  if (lb == krakenCurrencies.end()) {
    throw exception("Cannot find {} in Kraken currencies", m.quote());
  }
  CurrencyExchange krakenCurrencyExchangeQuote = *lb;
  string krakenAssetPair = krakenCurrencyExchangeBase.altStr();
  krakenAssetPair.append(krakenCurrencyExchangeQuote.altStr());
  json result = PublicQuery(_curlHandle, "/public/Depth", {{"pair", krakenAssetPair}, {"count", count}});
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
  Market krakenMarket(_tradableCurrenciesCache.get().getOrThrow(m.base()).altCode(),
                      _tradableCurrenciesCache.get().getOrThrow(m.quote()).altCode());
  json result = PublicQuery(_curlHandle, "/public/Ticker", {{"pair", krakenMarket.assetsPairStrUpper()}});
  for (const auto& [krakenAssetPair, details] : result.items()) {
    std::string_view last24hVol = details["v"][1].get<std::string_view>();
    std::string_view lastTickerPrice = details["c"][0].get<std::string_view>();
    return {MonetaryAmount(last24hVol, m.base()), MonetaryAmount(lastTickerPrice, m.quote())};
  }
  throw exception("Invalid data retrieved from ticker information");
}

LastTradesVector KrakenPublic::queryLastTrades(Market m, int) {
  Market krakenMarket(_tradableCurrenciesCache.get().getOrThrow(m.base()).altCode(),
                      _tradableCurrenciesCache.get().getOrThrow(m.quote()).altCode());
  json result = PublicQuery(_curlHandle, "/public/Trades", {{"pair", krakenMarket.assetsPairStrUpper()}});
  LastTradesVector ret;
  for (const json& det : result.front()) {
    MonetaryAmount price(det[0].get<std::string_view>(), m.quote());
    MonetaryAmount amount(det[1].get<std::string_view>(), m.base());
    int64_t millisecondsSinceEpoch = static_cast<int64_t>(det[2].get<double>() * 1000);
    TradeSide tradeSide = det[3].get<std::string_view>() == "b" ? TradeSide::kBuy : TradeSide::kSell;

    ret.emplace_back(tradeSide, amount, price, TimePoint(std::chrono::milliseconds(millisecondsSinceEpoch)));
  }
  std::ranges::sort(ret);
  return ret;
}

void KrakenPublic::updateCacheFile() const {
  const auto [withdrawalInfoMapsPtr, latestUpdate] = _withdrawalFeesCache.retrieve();
  if (withdrawalInfoMapsPtr) {
    using WithdrawalInfoMaps = KrakenPublic::WithdrawalFeesFunc::WithdrawalInfoMaps;

    const WithdrawalInfoMaps& withdrawalInfoMaps = *withdrawalInfoMapsPtr;

    json data;
    data["timeepoch"] = std::chrono::duration_cast<std::chrono::seconds>(latestUpdate.time_since_epoch()).count();
    for (const auto& [curCode, withdrawFee] : withdrawalInfoMaps.first) {
      string curCodeStr = curCode.str();
      data["assets"][curCodeStr]["min"] = withdrawalInfoMaps.second.find(curCode)->second.amountStr();
      data["assets"][curCodeStr]["fee"] = withdrawFee.amountStr();
    }
    GetKrakenWithdrawInfoFile(_coincenterInfo.dataDir()).write(data);
  }
}

}  // namespace cct::api
