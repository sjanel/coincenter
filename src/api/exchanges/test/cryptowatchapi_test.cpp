
#include "cryptowatchapi.hpp"

#include <gtest/gtest.h>

namespace cct {
namespace api {
TEST(CryptowatchAPITest, Basic) {
  CryptowatchAPI cryptowatchAPI;

  CryptowatchAPI::PricesPerMarketMap krakenPrices = cryptowatchAPI.queryAllPrices("kraken");
  CryptowatchAPI::PricesPerMarketMap bithumbPrices = cryptowatchAPI.queryAllPrices("bithumb");

  EXPECT_TRUE(krakenPrices.contains("BTCEUR"));
  EXPECT_TRUE(bithumbPrices.contains("ETHKRW"));
  EXPECT_EQ(*cryptowatchAPI.queryPrice("kraken", Market(CurrencyCode("BTC"), CurrencyCode("EUR"))),
            krakenPrices.find("BTCEUR")->second);
  EXPECT_EQ(*cryptowatchAPI.queryPrice("bithumb", Market(CurrencyCode("ETH"), CurrencyCode("KRW"))),
            bithumbPrices.find("ETHKRW")->second);
}

TEST(CryptowatchAPITest, IsFiatService) {
  CryptowatchAPI cryptowatchAPI;

  EXPECT_TRUE(cryptowatchAPI.queryIsCurrencyCodeFiat("EUR"));
  EXPECT_TRUE(cryptowatchAPI.queryIsCurrencyCodeFiat("KRW"));
  EXPECT_TRUE(cryptowatchAPI.queryIsCurrencyCodeFiat("USD"));
  EXPECT_FALSE(cryptowatchAPI.queryIsCurrencyCodeFiat("BTC"));
  EXPECT_FALSE(cryptowatchAPI.queryIsCurrencyCodeFiat("XRP"));
}
}  // namespace api
}  // namespace cct