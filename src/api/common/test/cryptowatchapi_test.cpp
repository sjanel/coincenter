
#include "cryptowatchapi.hpp"

#include <gtest/gtest.h>

#include "coincenterinfo.hpp"

namespace cct::api {

class CryptowatchAPITest : public ::testing::Test {
 protected:
  CryptowatchAPITest() : cryptowatchAPI(config) {}

  void SetUp() override {}
  void TearDown() override {}

  CoincenterInfo config;
  CryptowatchAPI cryptowatchAPI;
};

TEST_F(CryptowatchAPITest, Prices) {
  EXPECT_NE(cryptowatchAPI.queryPrice("kraken", Market("BTC", "EUR")), std::nullopt);
  EXPECT_NE(cryptowatchAPI.queryPrice("bithumb", Market("KRW", "ETH")), std::nullopt);
}

TEST_F(CryptowatchAPITest, IsFiatService) {
  EXPECT_TRUE(cryptowatchAPI.queryIsCurrencyCodeFiat("EUR"));
  EXPECT_TRUE(cryptowatchAPI.queryIsCurrencyCodeFiat("KRW"));
  EXPECT_TRUE(cryptowatchAPI.queryIsCurrencyCodeFiat("USD"));
  EXPECT_FALSE(cryptowatchAPI.queryIsCurrencyCodeFiat("BTC"));
  EXPECT_FALSE(cryptowatchAPI.queryIsCurrencyCodeFiat("XRP"));
}
}  // namespace cct::api