#include "exchangeinfo.hpp"

#include <gtest/gtest.h>

namespace cct {
namespace {
json CreateExchangeInfoJson() {
  json ret = R"({
                "asset": {
                        "allexclude": "AUD,CAD",
                        "withdrawexclude": "BTC,EUR"
                },
                "tradefees": {
                        "maker": "0.16",
                        "taker": "0.26"
                },
                "query": {
                        "minpublicquerydelayms": 666,
                        "minprivatequerydelayms": 450
                }
  })"_json;
  return ret;
}
}  // namespace

TEST(ExchangeInfo, ExcludedAssets) {
  ExchangeInfo exchangeInfo("kraken", CreateExchangeInfoJson());
  EXPECT_EQ(exchangeInfo.excludedCurrenciesAll(),
            ExchangeInfo::CurrencySet({CurrencyCode("AUD"), CurrencyCode("CAD")}));
  EXPECT_EQ(exchangeInfo.excludedCurrenciesWithdrawal(),
            ExchangeInfo::CurrencySet({CurrencyCode("BTC"), CurrencyCode("EUR")}));
}

TEST(ExchangeInfo, TradeFees) {
  ExchangeInfo exchangeInfo("kraken", CreateExchangeInfoJson());
  EXPECT_EQ(exchangeInfo.applyMakerFee(MonetaryAmount("120.5", CurrencyCode("ETH"))),
            MonetaryAmount("120.3072", CurrencyCode("ETH")));
  EXPECT_EQ(exchangeInfo.applyTakerFee(MonetaryAmount("2.356097", CurrencyCode("ETH"))),
            MonetaryAmount("2.3499711478", CurrencyCode("ETH")));
}

TEST(ExchangeInfo, Query) {
  ExchangeInfo exchangeInfo("kraken", CreateExchangeInfoJson());
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(exchangeInfo.minPublicQueryDelay()).count(), 666);
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(exchangeInfo.minPrivateQueryDelay()).count(), 450);
}

}  // namespace cct