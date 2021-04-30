#include "exchangeinfo.hpp"

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_mathhelpers.hpp"
#include "monetaryamount.hpp"

namespace cct {

ExchangeInfo::ExchangeInfo(std::string_view exchangeNameStr, const json &exchangeData) {
  // Load trade fees
  constexpr char kTradeFeesStr[] = "tradefees";
  if (!exchangeData.contains(kTradeFeesStr)) {
    throw exception("Unable to load trade fees for exchange " + std::string(exchangeNameStr));
  }
  const json &tradeFeesData = exchangeData[kTradeFeesStr];
  std::string_view makerStr = tradeFeesData["maker"].get<std::string_view>();
  std::string_view takerStr = tradeFeesData["taker"].get<std::string_view>();

  _generalMakerRatio = (MonetaryAmount("100") - MonetaryAmount(makerStr)) / 100;
  _generalTakerRatio = (MonetaryAmount("100") - MonetaryAmount(takerStr)) / 100;

  // Load asset config
  constexpr char kAssets[] = "asset";
  if (exchangeData.contains(kAssets)) {
    const json &assetData = exchangeData[kAssets];

    constexpr const char *const kSubAssetConfig[] = {"allexclude", "withdrawexclude"};
    CurrencySet *const pCurrencySetPerConfig[] = {std::addressof(_excludedCurrenciesAll),
                                                  std::addressof(_excludedCurrenciesWithdrawal)};
    const int kNbSub = std::end(pCurrencySetPerConfig) - std::begin(pCurrencySetPerConfig);
    for (int subIdx = 0; subIdx < kNbSub; ++subIdx) {
      if (assetData.contains(kSubAssetConfig[subIdx])) {
        // Don't make these json fields required, do nothing if not present
        const json &subData = assetData[kSubAssetConfig[subIdx]];
        CurrencySet &currencySet = *pCurrencySetPerConfig[subIdx];
        std::string currenciesCSV = subData;
        std::size_t first = 0;
        std::size_t last = currenciesCSV.find_first_of(',');
        while (last != std::string::npos) {
          currencySet.emplace(std::string_view(currenciesCSV.begin() + first, currenciesCSV.begin() + last));
          first = last + 1;
          last = currenciesCSV.find_first_of(',', first);
        }
        if (first != currenciesCSV.size()) {
          currencySet.emplace(std::string_view(currenciesCSV.begin() + first, currenciesCSV.end()));
        }
        for (CurrencyCode code : currencySet) {
          log::debug("{}: Adding {} as {}", exchangeNameStr, code.str(), kSubAssetConfig[subIdx]);
        }
        log::info("Loaded {} currencies in config", currencySet.size());
      }
    }
  }

  // Load query config
  constexpr char kQuery[] = "query";
  if (!exchangeData.contains(kQuery)) {
    throw exception("Unable to load query configuration for exchange " + std::string(exchangeNameStr));
  }
  const json &queryData = exchangeData[kQuery];
  _minPublicQueryDelay = std::chrono::milliseconds(queryData["minpublicquerydelayms"].get<int>());
  _minPrivateQueryDelay = std::chrono::milliseconds(queryData["minprivatequerydelayms"].get<int>());
  log::info("Loaded {} & {} ms as minimum time between two queries in config (public & private exchanges respectively)",
            std::chrono::duration_cast<std::chrono::milliseconds>(_minPublicQueryDelay).count(),
            std::chrono::duration_cast<std::chrono::milliseconds>(_minPrivateQueryDelay).count());
}

}  // namespace cct