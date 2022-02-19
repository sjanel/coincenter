#include "exchangeinfomap.hpp"

#include "cct_const.hpp"
#include "durationstring.hpp"
#include "exchangeinfoparser.hpp"

namespace cct {

ExchangeInfoMap ComputeExchangeInfoMap(const json &jsonData) {
  ExchangeInfoMap map;

  TopLevelOption assetTopLevelOption(jsonData, TopLevelOption::kAssetsOptionStr);
  TopLevelOption queryTopLevelOption(jsonData, TopLevelOption::kQueryOptionStr);
  TopLevelOption tradeFeesTopLevelOption(jsonData, TopLevelOption::kTradeFeesOptionStr);
  TopLevelOption withdrawTopLevelOption(jsonData, TopLevelOption::kWithdrawOptionStr);

  for (std::string_view exchangeName : kSupportedExchanges) {
    std::string_view makerStr = tradeFeesTopLevelOption.getStr(exchangeName, "maker");
    std::string_view takerStr = tradeFeesTopLevelOption.getStr(exchangeName, "taker");

    Duration publicAPIRate = queryTopLevelOption.getDuration(exchangeName, "publicAPIRate");
    Duration privateAPIRate = queryTopLevelOption.getDuration(exchangeName, "privateAPIRate");

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
         queryTopLevelOption.getDuration(exchangeName, kUpdFreqOptStr, "nbDecimals")}};

    bool multiTradeAllowedByDefault = queryTopLevelOption.getBool(exchangeName, "multiTradeAllowedByDefault");
    bool validateDepositAddressesInFile =
        withdrawTopLevelOption.getBool(exchangeName, "validateDepositAddressesInFile");
    bool placeSimulatedRealOrder = queryTopLevelOption.getBool(exchangeName, "placeSimulateRealOrder");

    map.insert_or_assign(
        exchangeName,
        ExchangeInfo(exchangeName, makerStr, takerStr,
                     assetTopLevelOption.getUnorderedCurrencyUnion(exchangeName, "allExclude"),
                     assetTopLevelOption.getUnorderedCurrencyUnion(exchangeName, "withdrawExclude"),
                     assetTopLevelOption.getCurrenciesArray(exchangeName, kPreferredPaymentCurrenciesOptName),
                     std::move(apiUpdateFrequencies), publicAPIRate, privateAPIRate, multiTradeAllowedByDefault,
                     validateDepositAddressesInFile, placeSimulatedRealOrder));
  }  // namespace cct

  return map;
}
}  // namespace cct