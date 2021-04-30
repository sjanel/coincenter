#include "exchangename.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"

namespace cct {
TEST(ExchangeName, NoKeyName) {
  EXPECT_EQ(PrivateExchangeName("binance").str(), "binance");
  EXPECT_EQ(PrivateExchangeName("kraken").name(), "kraken");
  EXPECT_EQ(PrivateExchangeName("bithumb").keyName(), "");
}

TEST(ExchangeName, SimpleKeyName) {
  EXPECT_EQ(PrivateExchangeName("binance_user1").str(), "binance_user1");
  EXPECT_EQ(PrivateExchangeName("kraken_user2").name(), "kraken");
  EXPECT_EQ(PrivateExchangeName("kraken_user3").keyName(), "user3");
}

TEST(ExchangeName, ComplexKeyName) {
  EXPECT_EQ(PrivateExchangeName("bithumb_complexUser_6KeyName_42").name(), "bithumb");
  EXPECT_EQ(PrivateExchangeName("bithumb_complexUser_KeyName_6").keyName(), "complexUser_KeyName_6");
  EXPECT_EQ(PrivateExchangeName("upbit__thisisaTrap_").keyName(), "_thisisaTrap_");
  EXPECT_EQ(PrivateExchangeName("upbit__thisisaTrap_").str(), "upbit__thisisaTrap_");
}

TEST(ExchangeName, ConstructorWith2Params) {
  EXPECT_EQ(PrivateExchangeName("binance", "_user13").str(), "binance__user13");
  EXPECT_THROW(PrivateExchangeName("kraken_", "_user13"), cct::exception);
}

TEST(ExchangeName, IsKeyNameDefined) {
  EXPECT_TRUE(PrivateExchangeName("binance", "_user13").isKeyNameDefined());
  EXPECT_FALSE(PrivateExchangeName("binance", "").isKeyNameDefined());
  EXPECT_TRUE(PrivateExchangeName("upbit__thisisaTrap_").isKeyNameDefined());
  EXPECT_FALSE(PrivateExchangeName("kraken").isKeyNameDefined());
}

}  // namespace cct