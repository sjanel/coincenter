#include "market.hpp"

#include <gtest/gtest.h>

#include <map>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"

namespace cct {
TEST(MarketTest, DefaultConstructor) {
  Market market;

  EXPECT_TRUE(market.base().isNeutral());
  EXPECT_TRUE(market.quote().isNeutral());
  EXPECT_TRUE(market.isNeutral());
  EXPECT_FALSE(market.isDefined());
  EXPECT_EQ(Market(), market);
}

TEST(MarketTest, CurrencyConstructor) {
  Market market(CurrencyCode("ETH"), "USDT");

  EXPECT_EQ(market.base(), CurrencyCode("ETH"));
  EXPECT_EQ(market.quote(), CurrencyCode("USDT"));
  EXPECT_FALSE(market.isNeutral());
  EXPECT_TRUE(market.isDefined());
  EXPECT_EQ(Market("eth", "usdt"), market);
}

TEST(MarketTest, StringConstructor) {
  Market market("sol-KRW");

  EXPECT_EQ(market.base(), CurrencyCode("SOL"));
  EXPECT_EQ(market.quote(), CurrencyCode("KRW"));
  EXPECT_EQ(Market("sol", "KRW"), market);
}

TEST(MarketTest, IncorrectStringConstructor) {
  EXPECT_THROW(Market("sol"), exception);
  EXPECT_THROW(Market("BTC-EUR-"), exception);
}

TEST(MarketTest, StringRepresentationRegularMarket) {
  Market market("shib", "btc");

  EXPECT_EQ(market.str(), "SHIB-BTC");
  EXPECT_EQ(market.assetsPairStrUpper('/'), "SHIB/BTC");
  EXPECT_EQ(market.assetsPairStrLower('|'), "shib|btc");
}

TEST(MarketTest, StringRepresentationFiatConversionMarket) {
  Market market("USDT", "EUR", Market::Type::kFiatConversionMarket);

  EXPECT_EQ(market.str(), "*USDT-EUR");
  EXPECT_EQ(market.assetsPairStrUpper('('), "*USDT(EUR");
  EXPECT_EQ(market.assetsPairStrLower(')'), "*usdt)eur");
}

TEST(MarketTest, StrLen) {
  Market market("shib", "btc");

  EXPECT_EQ(market.strLen(), 8);
  EXPECT_EQ(market.strLen(false), 7);
  EXPECT_EQ(market.strLen(true), 8);

  market = Market("1INCH", "EUR", Market::Type::kFiatConversionMarket);
  EXPECT_EQ(market.strLen(), 10);
}

struct Foo {
  bool operator==(const Foo &) const noexcept = default;

  Market market;
};

TEST(MarketTest, JsonSerializationValue) {
  Foo foo{Market{"DOGE", "BTC"}};

  string buffer;
  auto res = json::write<json::opts{.raw_string = true}>(foo, buffer);  // NOLINT(readability-implicit-bool-conversion)

  EXPECT_FALSE(res);

  EXPECT_EQ(buffer, R"({"market":"DOGE-BTC"})");
}

TEST(MarketTest, JsonSerializationKey) {
  std::map<Market, bool> map{{Market{"DOGE", "BTC"}, true}, {Market{"BTC", "ETH"}, false}};

  string buffer;
  auto res = json::write<json::opts{.raw_string = true}>(map, buffer);  // NOLINT(readability-implicit-bool-conversion)

  EXPECT_FALSE(res);

  EXPECT_EQ(buffer, R"({"BTC-ETH":false,"DOGE-BTC":true})");
}

struct Bar {
  vector<Market> markets{Market{"DOGE", "BTC"}, Market{"ETH", "KRW"}};
};

TEST(MarketTest, JsonSerializationVector) {
  Bar bar;

  string buffer;
  auto res = json::write<json::opts{.raw_string = true}>(bar, buffer);  // NOLINT(readability-implicit-bool-conversion)

  EXPECT_FALSE(res);

  EXPECT_EQ(buffer, R"({"markets":["DOGE-BTC","ETH-KRW"]})");
}

TEST(MarketTest, JsonDeserialization) {
  Foo foo;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::read<json::opts{.raw_string = true}>(foo, R"({"market":"DOGE-ETH"})");

  ASSERT_FALSE(ec);

  EXPECT_EQ(foo, Foo{Market("DOGE", "ETH")});
}

}  // namespace cct
