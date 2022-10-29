#include "exchangename.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"

namespace cct {
TEST(ExchangeName, NoKeyName) {
  EXPECT_EQ(ExchangeName("binance").str(), "binance");
  EXPECT_EQ(ExchangeName("kraken").name(), "kraken");
  EXPECT_EQ(ExchangeName("bithumb").keyName(), "");
  EXPECT_EQ(ExchangeName("KrakEn").keyName(), "");
}

TEST(ExchangeName, SimpleKeyName) {
  EXPECT_EQ(ExchangeName("binance_user1").str(), "binance_user1");
  EXPECT_EQ(ExchangeName("kraken_user2").name(), "kraken");
  EXPECT_EQ(ExchangeName("kraken_user3").keyName(), "user3");
  EXPECT_EQ(ExchangeName("huobi_USER3").keyName(), "USER3");
}

TEST(ExchangeName, ExchangeNameShouldBeLowerCase) {
  EXPECT_EQ(ExchangeName("Binance_user1").str(), "binance_user1");
  EXPECT_EQ(ExchangeName("Kraken").name(), "kraken");
  ExchangeName trickyName("UPBIT__thisisaTrap_");
  EXPECT_EQ(trickyName.name(), "upbit");
  EXPECT_EQ(trickyName.keyName(), "_thisisaTrap_");
}

TEST(ExchangeName, ComplexKeyName) {
  EXPECT_EQ(ExchangeName("bithumb_complexUser_6KeyName_42").name(), "bithumb");
  EXPECT_EQ(ExchangeName("bithumb_complexUser_KeyName_6").keyName(), "complexUser_KeyName_6");
  EXPECT_EQ(ExchangeName("upbit__thisisaTrap_").keyName(), "_thisisaTrap_");
  EXPECT_EQ(ExchangeName("upbit__thisisaTrap_").str(), "upbit__thisisaTrap_");
}

TEST(ExchangeName, ConstructorWith2Params) {
  EXPECT_EQ(ExchangeName("binance", "_user13").str(), "binance__user13");
  EXPECT_THROW(ExchangeName("kraken_", "_user13"), exception);
}

TEST(ExchangeName, IsKeyNameDefined) {
  EXPECT_TRUE(ExchangeName("binance", "_user13").isKeyNameDefined());
  EXPECT_FALSE(ExchangeName("binance", "").isKeyNameDefined());
  EXPECT_TRUE(ExchangeName("upbit__thisisaTrap_").isKeyNameDefined());
  EXPECT_FALSE(ExchangeName("kraken").isKeyNameDefined());
}

TEST(ExchangeName, Equality) {
  EXPECT_EQ(ExchangeName("binance", "_user13"), ExchangeName("BinanCE", "_user13"));
  EXPECT_NE(ExchangeName("binance", "_user13"), ExchangeName("inanCE", "_user13"));
  EXPECT_NE(ExchangeName("binance", "_user13"), ExchangeName("binance", "_uSer13"));
  EXPECT_NE(ExchangeName("upbit", "_user13"), ExchangeName("binance", "_user13"));
}

TEST(ExchangeName, Format) {
  EXPECT_EQ(fmt::format("{}", ExchangeName("binance_key")), "binance");
  EXPECT_EQ(fmt::format("{:e}", ExchangeName("binance_key")), "binance");
  EXPECT_EQ(fmt::format("{:n}", ExchangeName("binance_key")), "binance");
  EXPECT_EQ(fmt::format("{:k}", ExchangeName("binance_key")), "key");
  EXPECT_EQ(fmt::format("{:ek}", ExchangeName("binance_key")), "binance_key");
}

}  // namespace cct