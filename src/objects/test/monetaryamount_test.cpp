
#include "monetaryamount.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"

namespace cct {

TEST(MonetaryAmountTest, TwoDecimals) {
  CurrencyCode euroCode("EUR");
  const int nbDecimals = 2;

  MonetaryAmount mamount1(423, euroCode, nbDecimals);
  EXPECT_EQ(mamount1.currencyCode(), euroCode);
  EXPECT_EQ(mamount1.str(), "4.23 EUR");

  MonetaryAmount mamount2(-25, euroCode, nbDecimals);
  EXPECT_EQ(mamount2.str(), "-0.25 EUR");

  MonetaryAmount mamount3(7, euroCode, nbDecimals);
  EXPECT_EQ(mamount3.str(), "0.07 EUR");

  MonetaryAmount mamount4(-123450, euroCode, nbDecimals);
  EXPECT_EQ(mamount4.str(), "-1234.5 EUR");

  MonetaryAmount mamount5(4900, euroCode, nbDecimals);
  EXPECT_EQ(mamount5.nbDecimals(), 0);
  EXPECT_EQ(mamount5.str(), "49 EUR");
}

TEST(MonetaryAmountTest, TenDecimals) {
  CurrencyCode btcCode("BTC");
  const int nbDecimals = 10;

  MonetaryAmount mamount1(76491094, btcCode, nbDecimals);
  EXPECT_EQ(mamount1.integerPart(), 0);
  EXPECT_EQ(mamount1.str(), "0.0076491094 BTC");

  MonetaryAmount mamount2(-250034567346000, btcCode, nbDecimals);
  EXPECT_EQ(mamount2.nbDecimals(), 7);
  EXPECT_EQ(mamount2.integerPart(), -25003);
  EXPECT_EQ(mamount2.str(), "-25003.4567346 BTC");

  MonetaryAmount mamount3(0, btcCode, nbDecimals);
  EXPECT_TRUE(mamount3.isZero());
  EXPECT_EQ(mamount3.integerPart(), 0);
  EXPECT_EQ(mamount3.str(), "0 BTC");

  mamount1 = MonetaryAmount("0.0620089", btcCode);
  EXPECT_FALSE(mamount1.isZero());
  EXPECT_EQ(mamount1.nbDecimals(), 7);
  EXPECT_EQ(*MonetaryAmount("-314.451436574563", btcCode).amount(nbDecimals), -3144514365745);
  mamount1 = MonetaryAmount("-314.451436574563", btcCode);
  EXPECT_EQ(mamount1.nbDecimals(), 12);

  mamount1 = MonetaryAmount("2.0036500", btcCode);
  EXPECT_EQ(*mamount1.amount(2), 200);
  EXPECT_EQ(mamount1.integerPart(), 2);
  EXPECT_EQ(mamount1.nbDecimals(), 5);
}

TEST(MonetaryAmountTest, NoDecimals) {
  CurrencyCode krwCode("KRW");
  const int nbDecimals = 0;

  MonetaryAmount mamount1(250000000, krwCode, nbDecimals);
  EXPECT_EQ(mamount1.str(), "250000000 KRW");

  MonetaryAmount mamount2(-777, krwCode, nbDecimals);
  EXPECT_EQ(mamount2.str(), "-777 KRW");

  MonetaryAmount mamount3(0, krwCode, nbDecimals);
  EXPECT_EQ(mamount3.str(), "0 KRW");

  EXPECT_EQ(*MonetaryAmount("0.620089", krwCode).amount(nbDecimals), 0);
  EXPECT_EQ(*MonetaryAmount("-31415.0", krwCode).amount(nbDecimals), -31415);
  EXPECT_EQ(*MonetaryAmount("3", krwCode).amount(nbDecimals), 3);

  EXPECT_EQ(MonetaryAmount("35.620089", krwCode).amount(18), std::nullopt);
}

TEST(MonetaryAmountTest, Arithmetic) {
  CurrencyCode euroCode("EUR");

  MonetaryAmount lhs("3.14", euroCode);
  MonetaryAmount rhs("-2.7", euroCode);

  EXPECT_EQ(lhs + rhs, MonetaryAmount("0.44", euroCode));
  EXPECT_EQ(lhs - rhs, MonetaryAmount("5.84", euroCode));
  EXPECT_EQ(lhs - (-rhs), lhs + rhs);
  EXPECT_EQ(lhs * 2, -2 * -lhs);
  lhs += MonetaryAmount("-34.123", euroCode);
  EXPECT_EQ(lhs, MonetaryAmount("-30.983", euroCode));
  rhs -= MonetaryAmount("5069", euroCode);
  EXPECT_EQ(rhs, MonetaryAmount("-5071.7", euroCode));

  EXPECT_EQ(MonetaryAmount("0.49999999999976", "KRW") + MonetaryAmount("14183417.9174094504", "KRW"),
            MonetaryAmount("14183418.4174094503", "KRW"));
}

TEST(MonetaryAmountTest, Comparison) {
  EXPECT_LT(MonetaryAmount("0.49999999999976", "KRW"), MonetaryAmount("14183417.9174094504", "KRW"));
}

TEST(MonetaryAmountTest, OverflowProtection) {
  // OK to truncate decimal part
  EXPECT_LT(MonetaryAmount("94729475.1434000003456523423654", "EUR") - MonetaryAmount("94729475.1434", "EUR"),
            MonetaryAmount("0.0001", "EUR"));
  EXPECT_EQ(MonetaryAmount("123454562433254326.435324", "EUR"), MonetaryAmount("123454562433254326", "EUR"));

  // Should not accept truncation on integral part
  EXPECT_ANY_THROW(MonetaryAmount("1234545624332543260.435324", "EUR"));

  // Overflow on sum
  MonetaryAmount lhs("9472902.80094504728", "BTC");
  MonetaryAmount rhs("8577120.15", "BTC");
  EXPECT_EQ(lhs + rhs, MonetaryAmount("18050022.9509450472", "BTC"));  // last digit should be truncated (no rounding)
  EXPECT_EQ(lhs += rhs, MonetaryAmount("18050022.9509450472", "BTC"));

  // Overflow on sub
  lhs = MonetaryAmount("-9472902.80094504728", "BTC");
  rhs = MonetaryAmount("8577120.15", "BTC");
  EXPECT_EQ(lhs - rhs, MonetaryAmount("-18050022.9509450472", "BTC"));
  EXPECT_EQ(lhs -= rhs, MonetaryAmount("-18050022.9509450472", "BTC"));

  // Overflow on multiplication
  EXPECT_EQ(MonetaryAmount("-9472902.80094504728", "BTC") * 3, MonetaryAmount("-28418708.4028351416", "BTC"));
  EXPECT_EQ(MonetaryAmount("9472902.80094504728", "BTC") * -42, MonetaryAmount("-397861917.639691974", "BTC"));

  EXPECT_LT(MonetaryAmount("0.00326358030948980448 BTC"), MonetaryAmount("0.102 BTC"));
  EXPECT_EQ(MonetaryAmount("0.00427734447678 BTC") * MonetaryAmount("0.9974"),
            MonetaryAmount("0.00426622338114037 BTC"));
  EXPECT_EQ(MonetaryAmount("38.0566894350664") * MonetaryAmount("0.00008795 BTC"),
            MonetaryAmount("0.00334708583581405 BTC"));
  EXPECT_EQ(MonetaryAmount("0.00353598978800261 ETH") / MonetaryAmount("19.65 ETH"),
            MonetaryAmount("0.00017994858972023"));
  EXPECT_EQ(MonetaryAmount("0.00000598978800261 ETH") / MonetaryAmount("19.65 ETH"),
            MonetaryAmount("0.00000030482381692"));
  EXPECT_EQ(MonetaryAmount("0.00000598978800261 ETH") / 17, MonetaryAmount("0.00000035234047074 ETH"));
}

TEST(MonetaryAmountTest, Divide) {
  EXPECT_EQ(MonetaryAmount("1928", CurrencyCode("ETH")) / 100, MonetaryAmount("19.28", CurrencyCode("ETH")));
  EXPECT_EQ(MonetaryAmount("1928", CurrencyCode("ETH")) / 1000, MonetaryAmount("1.928", CurrencyCode("ETH")));
  EXPECT_EQ(MonetaryAmount("1928", CurrencyCode("ETH")) / 10000, MonetaryAmount("0.1928", CurrencyCode("ETH")));
  EXPECT_EQ(MonetaryAmount("1928", CurrencyCode("ETH")) / 100000, MonetaryAmount("0.01928", CurrencyCode("ETH")));

  EXPECT_EQ(MonetaryAmount("123.27", CurrencyCode("ETH")) / 3, MonetaryAmount("41.09", CurrencyCode("ETH")));
  EXPECT_EQ(MonetaryAmount("-870.5647", CurrencyCode("ETH")) / 577,
            MonetaryAmount("-1.50877764298093587", CurrencyCode("ETH")));

  EXPECT_EQ(MonetaryAmount("-870.5647", CurrencyCode("ETH")) /= 577,
            MonetaryAmount("-1.50877764298093587", CurrencyCode("ETH")));

  EXPECT_EQ(MonetaryAmount("1928", CurrencyCode("ETH")) / MonetaryAmount("100"),
            MonetaryAmount("19.28", CurrencyCode("ETH")));

  EXPECT_EQ(MonetaryAmount("123.27", CurrencyCode("ETH")) / MonetaryAmount("3.65"),
            MonetaryAmount("33.7726027397260273", CurrencyCode("ETH")));
  EXPECT_EQ(MonetaryAmount("-870.5647", CurrencyCode("ETH")) / MonetaryAmount("4709.3467736", CurrencyCode("ETH")),
            MonetaryAmount("-0.18485890758358997"));
  EXPECT_EQ(MonetaryAmount("487.76 EUR") / MonetaryAmount("1300.5 EUR"), MonetaryAmount("0.3750557477893118"));
  EXPECT_THROW(MonetaryAmount("100") / MonetaryAmount("0.00000000000000001"), exception);
  EXPECT_EQ(MonetaryAmount("10") / MonetaryAmount("0.0000000000000001"), MonetaryAmount("100000000000000000"));
}

TEST(MonetaryAmountTest, Multiply) {
  EXPECT_EQ(MonetaryAmount("3.25", CurrencyCode("ETH")) * MonetaryAmount("4.578"),
            MonetaryAmount("14.8785", CurrencyCode("ETH")));
  EXPECT_EQ(MonetaryAmount("79871.9000917457") * MonetaryAmount("-34.141590974"),
            MonetaryAmount("-2726953.66542788469"));
  EXPECT_THROW(MonetaryAmount("1", "EUR") * MonetaryAmount("2", "ETH"), exception);
}

TEST(MonetaryAmountTest, Convert) {
  EXPECT_EQ(MonetaryAmount("2", "ETH").convertTo(MonetaryAmount("1600", "EUR")), MonetaryAmount("3200", "EUR"));
  EXPECT_EQ(MonetaryAmount("1500", "EUR").convertTo(MonetaryAmount("0.0005", "ETH")), MonetaryAmount("0.75", "ETH"));
}

TEST(MonetaryAmountTest, StringConstructor) {
  EXPECT_EQ(MonetaryAmount("804.62EUR"), MonetaryAmount("804.62", "EUR"));
  EXPECT_EQ(MonetaryAmount("-210.50 CAKE"), MonetaryAmount("-210.50", "CAKE"));
  EXPECT_EQ(MonetaryAmount("05AUD"), MonetaryAmount("5", "AUD"));
  EXPECT_EQ(MonetaryAmount("746REPV2"), MonetaryAmount("746", "REPV2"));
}

TEST(MonetaryAmountTest, Zero) {
  EXPECT_TRUE(MonetaryAmount("0EUR").isZero());
  EXPECT_FALSE(MonetaryAmount("0.0001EUR").isZero());
}

TEST(MonetaryAmountTest, StepRounding) {
  EXPECT_EQ(MonetaryAmount("12.35 EUR").round(MonetaryAmount("0.1"), MonetaryAmount::RoundType::kDown),
            MonetaryAmount("12.3 EUR"));
  EXPECT_EQ(MonetaryAmount("12.35 EUR").round(MonetaryAmount("0.1"), MonetaryAmount::RoundType::kUp),
            MonetaryAmount("12.4 EUR"));
  EXPECT_EQ(MonetaryAmount("-23.5 EUR").round(MonetaryAmount("0.5"), MonetaryAmount::RoundType::kDown),
            MonetaryAmount("-23.5 EUR"));
  EXPECT_EQ(MonetaryAmount("-23.5 EUR").round(MonetaryAmount("1"), MonetaryAmount::RoundType::kDown),
            MonetaryAmount("-24 EUR"));
  EXPECT_EQ(MonetaryAmount("-927.47 EUR").round(MonetaryAmount("0.007"), MonetaryAmount::RoundType::kUp),
            MonetaryAmount("-927.465 EUR"));

  EXPECT_EQ(MonetaryAmount("12.34 EUR").round(MonetaryAmount("0.1"), MonetaryAmount::RoundType::kNearest),
            MonetaryAmount("12.3 EUR"));
  EXPECT_EQ(MonetaryAmount("12.58 EUR").round(MonetaryAmount("0.1"), MonetaryAmount::RoundType::kNearest),
            MonetaryAmount("12.6 EUR"));
  EXPECT_EQ(MonetaryAmount("12.5 EUR").round(MonetaryAmount("1"), MonetaryAmount::RoundType::kNearest),
            MonetaryAmount("13 EUR"));
  EXPECT_EQ(MonetaryAmount("12.5 EUR").round(MonetaryAmount("0.5"), MonetaryAmount::RoundType::kNearest),
            MonetaryAmount("12.5 EUR"));
  EXPECT_EQ(MonetaryAmount("-23.5 EUR").round(MonetaryAmount("0.5"), MonetaryAmount::RoundType::kNearest),
            MonetaryAmount("-23.5 EUR"));
  EXPECT_EQ(MonetaryAmount("-23.5 EUR").round(MonetaryAmount("1"), MonetaryAmount::RoundType::kNearest),
            MonetaryAmount("-24 EUR"));
  EXPECT_EQ(MonetaryAmount("-23.6 EUR").round(MonetaryAmount("1"), MonetaryAmount::RoundType::kNearest),
            MonetaryAmount("-24 EUR"));
  EXPECT_EQ(MonetaryAmount("-23.1 EUR").round(MonetaryAmount("1"), MonetaryAmount::RoundType::kNearest),
            MonetaryAmount("-23 EUR"));
}

TEST(MonetaryAmountTest, PositiveDoubleConstructor) {
  EXPECT_EQ(MonetaryAmount(2.267E-5), MonetaryAmount("0.00002267"));
  EXPECT_EQ(MonetaryAmount(121.80267966), MonetaryAmount("121.80267966"));
  EXPECT_EQ(MonetaryAmount(482.33134464), MonetaryAmount("482.33134464"));
  EXPECT_EQ(MonetaryAmount(7721.65024864), MonetaryAmount("7721.65024864"));
  EXPECT_EQ(MonetaryAmount(8.0E-4), MonetaryAmount("0.0008"));
  EXPECT_EQ(MonetaryAmount(7.9999E-4), MonetaryAmount("0.00079999"));
  EXPECT_EQ(MonetaryAmount(0.14), MonetaryAmount("0.14"));
  EXPECT_EQ(MonetaryAmount(0.14000001), MonetaryAmount("0.14000001"));
  EXPECT_EQ(MonetaryAmount(700.00000011), MonetaryAmount("700.00000011"));
  EXPECT_EQ(MonetaryAmount(700.0), MonetaryAmount("700"));
  EXPECT_EQ(MonetaryAmount(0.98300003), MonetaryAmount("0.98300003"));
  EXPECT_EQ(MonetaryAmount(0.98300002), MonetaryAmount("0.98300002"));
  EXPECT_EQ(MonetaryAmount(0.98300001), MonetaryAmount("0.98300001"));
  EXPECT_EQ(MonetaryAmount(0.983), MonetaryAmount("0.983"));
  EXPECT_EQ(MonetaryAmount(119999.52864837), MonetaryAmount("119999.52864837"));
}

TEST(MonetaryAmountTest, NegativeDoubleConstructor) {
  EXPECT_EQ(MonetaryAmount(-2.267E-5), MonetaryAmount("-0.00002267"));
  EXPECT_EQ(MonetaryAmount(-121.80267966), MonetaryAmount("-121.80267966"));
  EXPECT_EQ(MonetaryAmount(-482.33134464), MonetaryAmount("-482.33134464"));
  EXPECT_EQ(MonetaryAmount(-7721.65024864), MonetaryAmount("-7721.65024864"));
  EXPECT_EQ(MonetaryAmount(-8.0E-4), MonetaryAmount("-0.0008"));
  EXPECT_EQ(MonetaryAmount(-7.9999E-4), MonetaryAmount("-0.00079999"));
  EXPECT_EQ(MonetaryAmount(-0.14), MonetaryAmount("-0.14"));
  EXPECT_EQ(MonetaryAmount(-0.14000001), MonetaryAmount("-0.14000001"));
}

TEST(MonetaryAmountTest, Truncate) {
  MonetaryAmount a("0.00008244");
  a.truncate(6);
  EXPECT_EQ(a, MonetaryAmount("0.000082"));
  a.truncate(4);
  EXPECT_EQ(a, MonetaryAmount("0"));
  EXPECT_TRUE(a.isZero());
  EXPECT_EQ(
      MonetaryAmount(std::numeric_limits<MonetaryAmount::AmountType>::max(), CurrencyCode(), 18)
          .round(MonetaryAmount(1, CurrencyCode(), 4), MonetaryAmount::RoundType::kNearest),
      MonetaryAmount(std::numeric_limits<MonetaryAmount::AmountType>::max() / ipow(10, 14) + 1L, CurrencyCode(), 4));
  EXPECT_EQ(
      MonetaryAmount(std::numeric_limits<MonetaryAmount::AmountType>::min(), CurrencyCode(), 18)
          .round(MonetaryAmount(1, CurrencyCode(), 4), MonetaryAmount::RoundType::kDown),
      MonetaryAmount(std::numeric_limits<MonetaryAmount::AmountType>::min() / ipow(10, 14) - 1L, CurrencyCode(), 4));
}

TEST(MonetaryAmountTest, PositiveAmountStr) {
  EXPECT_EQ(MonetaryAmount("7").amountStr(), "7");
  EXPECT_EQ(MonetaryAmount("9204.1260").amountStr(), "9204.126");
  EXPECT_EQ(MonetaryAmount("0.709").amountStr(), "0.709");
  EXPECT_EQ(MonetaryAmount("0.0").amountStr(), "0");
}

TEST(MonetaryAmountTest, NegativeAmountStr) {
  EXPECT_EQ(MonetaryAmount("-3.4950EUR").amountStr(), "-3.495");
  EXPECT_EQ(MonetaryAmount("-0.0034090").amountStr(), "-0.003409");
  EXPECT_EQ(MonetaryAmount("-0.0").amountStr(), "0");
}

}  // namespace cct