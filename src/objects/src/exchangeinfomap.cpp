#include "exchangeinfomap.hpp"

#include "cct_const.hpp"
#include "durationstring.hpp"
#include "exchangeinfoparser.hpp"

namespace cct {
namespace {
vector<CurrencyCode> ConvertToVector(std::string_view csvAssets) {
  std::size_t first = 0;
  std::size_t last = csvAssets.find(',');
  vector<CurrencyCode> v;
  while (last != std::string_view::npos) {
    v.emplace_back(std::string_view(csvAssets.begin() + first, csvAssets.begin() + last));
    first = last + 1;
    last = csvAssets.find(',', first);
  }
  if (first != csvAssets.size()) {
    v.emplace_back(std::string_view(csvAssets.begin() + first, csvAssets.end()));
  }
  return v;
}
}  // namespace

ExchangeInfoMap ComputeExchangeInfoMap(const json &jsonData) {
  ExchangeInfoMap map;

  TopLevelOption assetTopLevelOption(jsonData, TopLevelOption::kAssetsOptionStr);
  TopLevelOption queryTopLevelOption(jsonData, TopLevelOption::kQueryOptionStr);
  TopLevelOption tradeFeesTopLevelOption(jsonData, TopLevelOption::kTradeFeesOptionStr);
  TopLevelOption withdrawTopLevelOption(jsonData, TopLevelOption::kWithdrawOptionStr);

  for (std::string_view exchangeName : kSupportedExchanges) {
    std::string_view makerStr = tradeFeesTopLevelOption.getStr(exchangeName, "maker");
    std::string_view takerStr = tradeFeesTopLevelOption.getStr(exchangeName, "taker");

    auto excludedAllCurrencies = ConvertToVector(assetTopLevelOption.getStrUnion(exchangeName, "allExclude"));
    auto excludedCurrenciesWithdraw = ConvertToVector(assetTopLevelOption.getStrUnion(exchangeName, "withdrawExclude"));

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

    bool validateDepositAddressesInFile =
        withdrawTopLevelOption.getBool(exchangeName, "validateDepositAddressesInFile");
    bool placeSimulatedRealOrder = queryTopLevelOption.getBool(exchangeName, "placeSimulateRealOrder");

    map.insert_or_assign(
        exchangeName, ExchangeInfo(exchangeName, makerStr, takerStr, excludedAllCurrencies, excludedCurrenciesWithdraw,
                                   std::move(apiUpdateFrequencies), publicAPIRate, privateAPIRate,
                                   validateDepositAddressesInFile, placeSimulatedRealOrder));
  }  // namespace cct

  return map;
}
}  // namespace cct