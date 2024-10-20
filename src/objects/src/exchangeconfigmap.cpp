#include "exchangeconfigmap.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

#include "cct_const.hpp"
#include "cct_json-container.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "exchangeconfig.hpp"
#include "exchangeconfigdefault.hpp"
#include "exchangeconfigparser.hpp"
#include "http-config.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "parseloglevel.hpp"
#include "priceoptionsdef.hpp"
#include "timedef.hpp"
#include "tradeconfig.hpp"
#include "tradedefinitions.hpp"

namespace cct {

ExchangeConfigMap ComputeExchangeConfigMap(std::string_view fileName, const json::container &jsonData) {
  ExchangeConfigMap map;

  const json::container &prodDefault = ExchangeConfigDefault::Prod();

  TopLevelOption assetTopLevelOption(TopLevelOption::kAssetsOptionStr, prodDefault, jsonData);
  TopLevelOption queryTopLevelOption(TopLevelOption::kQueryOptionStr, prodDefault, jsonData);
  TopLevelOption tradeFeesTopLevelOption(TopLevelOption::kTradeFeesOptionStr, prodDefault, jsonData);
  TopLevelOption withdrawTopLevelOption(TopLevelOption::kWithdrawOptionStr, prodDefault, jsonData);

  for (std::string_view exchangeName : kSupportedExchanges) {
    std::string_view makerStr = tradeFeesTopLevelOption.getStr(exchangeName, "maker");
    std::string_view takerStr = tradeFeesTopLevelOption.getStr(exchangeName, "taker");

    Duration publicAPIRate = queryTopLevelOption.getDuration(exchangeName, "publicAPIRate");
    Duration privateAPIRate = queryTopLevelOption.getDuration(exchangeName, "privateAPIRate");

    std::string_view acceptEncoding = queryTopLevelOption.getStr(exchangeName, "acceptEncoding");

    static constexpr std::string_view kUpdFreqOptStr = "updateFrequency";

    ExchangeConfig::APIUpdateFrequencies apiUpdateFrequencies{
        {queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "currencies"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "markets"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "withdrawalFees"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "allOrderbooks"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "orderbook"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "tradedVolume"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "lastPrice"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "depositWallet"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "currencyInfo")}};

    const bool multiTradeAllowedByDefault = queryTopLevelOption.getBool(exchangeName, "multiTradeAllowedByDefault");
    const bool validateDepositAddressesInFile =
        withdrawTopLevelOption.getBool(exchangeName, "validateDepositAddressesInFile");
    const bool placeSimulatedRealOrder = queryTopLevelOption.getBool(exchangeName, "placeSimulateRealOrder");
    const bool validateApiKey = queryTopLevelOption.getBool(exchangeName, "validateApiKey");
    const ExchangeConfig::MarketDataSerialization marketDataSerialization =
        queryTopLevelOption.getBool(exchangeName, "marketDataSerialization")
            ? ExchangeConfig::MarketDataSerialization::kYes
            : ExchangeConfig::MarketDataSerialization::kNo;

    MonetaryAmountByCurrencySet dustAmountsThresholds(
        queryTopLevelOption.getMonetaryAmountsArray(exchangeName, "dustAmountsThreshold"));
    const int dustSweeperMaxNbTrades = queryTopLevelOption.getInt(exchangeName, "dustSweeperMaxNbTrades");

    const auto requestsCallLogLevel =
        LevelFromPos(LogPosFromLogStr(queryTopLevelOption.getStr(exchangeName, "logLevels", "requestsCall")));
    const auto requestsAnswerLogLevel =
        LevelFromPos(LogPosFromLogStr(queryTopLevelOption.getStr(exchangeName, "logLevels", "requestsAnswer")));

    static constexpr std::string_view kTradeConfigPart = "trade";

    bool tradeTimeoutMatch = queryTopLevelOption.getBool(exchangeName, kTradeConfigPart, "timeoutMatch");
    TradeConfig tradeConfig(queryTopLevelOption.getDuration(exchangeName, kTradeConfigPart, "minPriceUpdateDuration"),
                            queryTopLevelOption.getDuration(exchangeName, kTradeConfigPart, "timeout"),
                            StrategyFromStr(queryTopLevelOption.getStr(exchangeName, kTradeConfigPart, "strategy")),
                            tradeTimeoutMatch ? TradeTimeoutAction::kMatch : TradeTimeoutAction::kCancel);

    static constexpr std::string_view kHttpConfigPart = "http";

    const auto httpTimeout = queryTopLevelOption.getDuration(exchangeName, kHttpConfigPart, "timeout");
    HttpConfig httpConfig(httpTimeout);

    map.insert_or_assign(
        exchangeName,
        ExchangeConfig(exchangeName, makerStr, takerStr,
                       assetTopLevelOption.getUnorderedCurrencyUnion(exchangeName, "allExclude"),
                       assetTopLevelOption.getUnorderedCurrencyUnion(exchangeName, "withdrawExclude"),
                       assetTopLevelOption.getCurrenciesArray(exchangeName, kPreferredPaymentCurrenciesOptName),
                       std::move(dustAmountsThresholds), std::move(apiUpdateFrequencies), publicAPIRate, privateAPIRate,
                       acceptEncoding, dustSweeperMaxNbTrades, requestsCallLogLevel, requestsAnswerLogLevel,
                       multiTradeAllowedByDefault, validateDepositAddressesInFile, placeSimulatedRealOrder,
                       validateApiKey, std::move(tradeConfig), std::move(httpConfig), marketDataSerialization));
  }  // namespace cct

  // Print json unused values
  json::container readValues;

  readValues.emplace(TopLevelOption::kAssetsOptionStr, assetTopLevelOption.getReadValues());
  readValues.emplace(TopLevelOption::kQueryOptionStr, queryTopLevelOption.getReadValues());
  readValues.emplace(TopLevelOption::kTradeFeesOptionStr, tradeFeesTopLevelOption.getReadValues());
  readValues.emplace(TopLevelOption::kWithdrawOptionStr, withdrawTopLevelOption.getReadValues());

  json::container diffJson = json::container::diff(jsonData, readValues);

  for (json::container &diffElem : diffJson) {
    std::string_view diffType = diffElem["op"].get<std::string_view>();
    string jsonPath = std::move(diffElem["path"].get_ref<string &>());

    std::ranges::replace(jsonPath, '/', '.');

    if (diffType == "add") {
      log::warn("Using default value for '{}' in '{}' - fill it explicitly to silent this warning", jsonPath, fileName);
    } else if (diffType == "remove") {
      log::warn("Unread data at path '{}' in '{}' - could be safely removed", jsonPath, fileName);
    } else {
      log::error("Unexpected difference '{}' at path '{}' in '{}'", diffType, jsonPath, fileName);
    }
  }

  return map;
}
}  // namespace cct
