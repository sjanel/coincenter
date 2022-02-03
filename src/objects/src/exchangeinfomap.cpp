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

    auto excludedAllCurrencies = ConvertToVector(assetTopLevelOption.getCSVUnion(exchangeName, "allexclude"));
    auto excludedCurrenciesWithdraw = ConvertToVector(assetTopLevelOption.getCSVUnion(exchangeName, "withdrawexclude"));

    int minPublicQueryDelayMs = queryTopLevelOption.getInt(exchangeName, "minpublicquerydelayms");
    int minPrivateQueryDelayMs = queryTopLevelOption.getInt(exchangeName, "minprivatequerydelayms");

    static constexpr std::string_view kUpdFreqOptStr = "updateFrequency";

    ExchangeInfo::APIUpdateFrequencies apiUpdateFrequencies{
        {ParseDuration(queryTopLevelOption.getStr(exchangeName, kUpdFreqOptStr, "currencies")),
         ParseDuration(queryTopLevelOption.getStr(exchangeName, kUpdFreqOptStr, "markets")),
         ParseDuration(queryTopLevelOption.getStr(exchangeName, kUpdFreqOptStr, "withdrawalFees")),
         ParseDuration(queryTopLevelOption.getStr(exchangeName, kUpdFreqOptStr, "allOrderbooks")),
         ParseDuration(queryTopLevelOption.getStr(exchangeName, kUpdFreqOptStr, "orderbook")),
         ParseDuration(queryTopLevelOption.getStr(exchangeName, kUpdFreqOptStr, "tradedVolume")),
         ParseDuration(queryTopLevelOption.getStr(exchangeName, kUpdFreqOptStr, "lastPrice")),
         ParseDuration(queryTopLevelOption.getStr(exchangeName, kUpdFreqOptStr, "depositWallet")),
         ParseDuration(queryTopLevelOption.getStr(exchangeName, kUpdFreqOptStr, "nbDecimals"))}};

    bool validateDepositAddressesInFile =
        withdrawTopLevelOption.getBool(exchangeName, "validatedepositaddressesinfile");
    bool placeSimulatedRealOrder = queryTopLevelOption.getBool(exchangeName, "placesimulaterealorder");

    map.insert_or_assign(
        exchangeName, ExchangeInfo(exchangeName, makerStr, takerStr, excludedAllCurrencies, excludedCurrenciesWithdraw,
                                   std::move(apiUpdateFrequencies), minPublicQueryDelayMs, minPrivateQueryDelayMs,
                                   validateDepositAddressesInFile, placeSimulatedRealOrder));
  }  // namespace cct

  return map;
}
}  // namespace cct