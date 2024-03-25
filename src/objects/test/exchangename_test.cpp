#include "exchangename.hpp"

#include <gtest/gtest.h>

#include "cct_format.hpp"
#include "cct_invalid_argument_exception.hpp"

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

TEST(ExchangeName, ConstructorWith1ParamInvalid) {
  EXPECT_THROW(ExchangeName("huobi_"), invalid_argument);
  EXPECT_THROW(ExchangeName("unknown"), invalid_argument);
  EXPECT_THROW(ExchangeName("ucoin_user1"), invalid_argument);
}

TEST(ExchangeName, ConstructorWith2ParamsValid) {
  EXPECT_EQ(ExchangeName("upbit", "user1").str(), "upbit_user1");
  EXPECT_EQ(ExchangeName("binance", "_user13").str(), "binance__user13");
}

TEST(ExchangeName, ConstructorWith2ParamsInvalid) {
  EXPECT_THROW(ExchangeName("kraken_", "_user13"), invalid_argument);
  EXPECT_THROW(ExchangeName("unknown", "user1"), invalid_argument);
}

TEST(ExchangeName, IsKeyNameDefined) {
  EXPECT_TRUE(ExchangeName("binance", "_user13").isKeyNameDefined());
  EXPECT_FALSE(ExchangeName("binance", "").isKeyNameDefined());
  EXPECT_TRUE(ExchangeName("upbit__thisisaTrap_").isKeyNameDefined());
  EXPECT_FALSE(ExchangeName("kraken").isKeyNameDefined());
}

TEST(ExchangeName, Equality) {
  EXPECT_EQ(ExchangeName("binance", "_user13"), ExchangeName("BinanCE", "_user13"));
  EXPECT_NE(ExchangeName("kucoin", "_user13"), ExchangeName("huobi", "_user13"));
  EXPECT_NE(ExchangeName("binance", "_user13"), ExchangeName("binance", "_uSer13"));
  EXPECT_NE(ExchangeName("upbit", "_user13"), ExchangeName("binance", "_user13"));
}

TEST(ExchangeName, FormatWithoutKey) {
  ExchangeName en("kraken");
  EXPECT_EQ(format("{}", en), "kraken");
  EXPECT_EQ(format("{:e}", en), "kraken");
  EXPECT_EQ(format("{:n}", en), "kraken");
  EXPECT_EQ(format("{:k}", en), "");
  EXPECT_EQ(format("{:ek}", en), "kraken");
}

TEST(ExchangeName, FormatWithKey) {
  ExchangeName en("binance_key");
  EXPECT_EQ(format("{}", en), "binance_key");
  EXPECT_EQ(format("{:e}", en), "binance");
  EXPECT_EQ(format("{:n}", en), "binance");
  EXPECT_EQ(format("{:k}", en), "key");
  EXPECT_EQ(format("{:ek}", en), "binance_key");
}

}  // namespace cct