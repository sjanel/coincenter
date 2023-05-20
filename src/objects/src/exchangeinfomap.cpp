#include "exchangeinfomap.hpp"

#include <algorithm>

#include "cct_const.hpp"
#include "durationstring.hpp"
#include "exchangeinfodefault.hpp"
#include "exchangeinfoparser.hpp"

namespace cct {

ExchangeInfoMap ComputeExchangeInfoMap(std::string_view fileName, const json &jsonData) {
  ExchangeInfoMap map;

  const json &prodDefault = ExchangeInfoDefault::Prod();

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

    ExchangeInfo::APIUpdateFrequencies apiUpdateFrequencies{
        {queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "currencies"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "markets"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "withdrawalFees"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "allOrderbooks"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "orderbook"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "tradedVolume"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "lastPrice"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "depositWallet"),
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "currencyInfo")}};

    bool multiTradeAllowedByDefault = queryTopLevelOption.getBool(exchangeName, "multiTradeAllowedByDefault");
    bool validateDepositAddressesInFile =
        withdrawTopLevelOption.getBool(exchangeName, "validateDepositAddressesInFile");
    bool placeSimulatedRealOrder = queryTopLevelOption.getBool(exchangeName, "placeSimulateRealOrder");
    bool validateApiKey = queryTopLevelOption.getBool(exchangeName, "validateApiKey");

    MonetaryAmountByCurrencySet dustAmountsThresholds(
        queryTopLevelOption.getMonetaryAmountsArray(exchangeName, "dustAmountsThreshold"));
    int dustSweeperMaxNbTrades = queryTopLevelOption.getInt(exchangeName, "dustSweeperMaxNbTrades");

    map.insert_or_assign(
        exchangeName,
        ExchangeInfo(exchangeName, makerStr, takerStr,
                     assetTopLevelOption.getUnorderedCurrencyUnion(exchangeName, "allExclude"),
                     assetTopLevelOption.getUnorderedCurrencyUnion(exchangeName, "withdrawExclude"),
                     assetTopLevelOption.getCurrenciesArray(exchangeName, kPreferredPaymentCurrenciesOptName),
                     std::move(dustAmountsThresholds), std::move(apiUpdateFrequencies), publicAPIRate, privateAPIRate,
                     acceptEncoding, dustSweeperMaxNbTrades, multiTradeAllowedByDefault, validateDepositAddressesInFile,
                     placeSimulatedRealOrder, validateApiKey));
  }  // namespace cct

  // Print json unused values
  json readValues;

  readValues.emplace(TopLevelOption::kAssetsOptionStr, assetTopLevelOption.getReadValues());
  readValues.emplace(TopLevelOption::kQueryOptionStr, queryTopLevelOption.getReadValues());
  readValues.emplace(TopLevelOption::kTradeFeesOptionStr, tradeFeesTopLevelOption.getReadValues());
  readValues.emplace(TopLevelOption::kWithdrawOptionStr, withdrawTopLevelOption.getReadValues());

  json diffJson = json::diff(jsonData, readValues);

  for (json &diffElem : diffJson) {
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