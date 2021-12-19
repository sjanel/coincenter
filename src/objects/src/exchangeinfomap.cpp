#include "exchangeinfomap.hpp"

#include "cct_const.hpp"
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

ExchangeInfoMap ComputeExchangeInfoMap(std::string_view dataDir) {
  ExchangeInfoMap map;

  constexpr std::string_view kAllTopLevelOptionNames[] = {
      TopLevelOption::kAssetsOptionStr, TopLevelOption::kQueryOptionStr, TopLevelOption::kTradeFeesOptionStr,
      TopLevelOption::kWithdrawOptionStr};
  json jsonData = LoadExchangeConfigData(dataDir, kAllTopLevelOptionNames);

  TopLevelOption assetTopLevelOption(jsonData, TopLevelOption::kAssetsOptionStr);
  TopLevelOption queryTopLevelOption(jsonData, TopLevelOption::kQueryOptionStr);
  TopLevelOption tradeFeesTopLevelOption(jsonData, TopLevelOption::kTradeFeesOptionStr);
  TopLevelOption withdrawTopLevelOption(jsonData, TopLevelOption::kWithdrawOptionStr);

  for (std::string_view exchangeName : kSupportedExchanges) {
    std::string_view makerStr = tradeFeesTopLevelOption.getBottomUp<std::string_view>(exchangeName, "maker", "0");
    std::string_view takerStr = tradeFeesTopLevelOption.getBottomUp<std::string_view>(exchangeName, "taker", "0");

    auto excludedAllCurrencies = ConvertToVector(assetTopLevelOption.getCSVUnion(exchangeName, "allexclude"));
    auto excludedCurrenciesWithdraw = ConvertToVector(assetTopLevelOption.getCSVUnion(exchangeName, "withdrawexclude"));

    int minPublicQueryDelayMs = queryTopLevelOption.getBottomUp<int>(exchangeName, "minpublicquerydelayms", 2000);
    int minPrivateQueryDelayMs = queryTopLevelOption.getBottomUp<int>(exchangeName, "minprivatequerydelayms", 2000);

    bool validateDepositAddressesInFile =
        withdrawTopLevelOption.getBottomUp<bool>(exchangeName, "validatedepositaddressesinfile", true);
    bool placeSimulatedRealOrder = queryTopLevelOption.getBottomUp<bool>(exchangeName, "placesimulaterealorder");

    map.insert_or_assign(string(exchangeName),
                         ExchangeInfo(exchangeName, makerStr, takerStr, excludedAllCurrencies,
                                      excludedCurrenciesWithdraw, minPublicQueryDelayMs, minPrivateQueryDelayMs,
                                      validateDepositAddressesInFile, placeSimulatedRealOrder));
  }

  return map;
}
}  // namespace cct