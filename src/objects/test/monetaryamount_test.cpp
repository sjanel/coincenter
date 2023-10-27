#include "monetaryamount.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <optional>

#include "cct_exception.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "mathhelpers.hpp"

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
  EXPECT_EQ(mamount3, 0);
  EXPECT_EQ(mamount3.integerPart(), 0);
  EXPECT_EQ(mamount3.str(), "0 BTC");

  mamount1 = MonetaryAmount("0.0620089", btcCode);
  EXPECT_NE(mamount1, 0);
  EXPECT_EQ(mamount1.nbDecimals(), 7);
  EXPECT_EQ(MonetaryAmount("-314.451436574563", btcCode).amount(nbDecimals).value_or(-1), -3144514365745);
  mamount1 = MonetaryAmount("-314.451436574563", btcCode);
  EXPECT_EQ(mamount1.nbDecimals(), 12);

  mamount1 = MonetaryAmount("2.0036500", btcCode);
  EXPECT_EQ(mamount1.amount(2).value_or(-1), 200);
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

  EXPECT_EQ(MonetaryAmount("0.620089", krwCode).amount(nbDecimals).value_or(-1), 0);
  EXPECT_EQ(MonetaryAmount("-31415.0", krwCode).amount(nbDecimals).value_or(-1), -31415);

  EXPECT_EQ(MonetaryAmount(3, krwCode).amount(nbDecimals).value_or(-1), 3);

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

  EXPECT_EQ(MonetaryAmount() + MonetaryAmount("3.1415 EUR"), MonetaryAmount("3.1415 EUR"));
  EXPECT_EQ(MonetaryAmount("3.1415 EUR") - MonetaryAmount(), MonetaryAmount("3.1415 EUR"));
}

TEST(MonetaryAmountTest, Comparison) {
  EXPECT_LT(MonetaryAmount("0.49999999999976", "KRW"), MonetaryAmount("14183417.9174094504", "KRW"));
  EXPECT_LT(MonetaryAmount("0.00326358030948980448", "EUR"), MonetaryAmount("0.102", "EUR"));
  EXPECT_LT(MonetaryAmount("0.00326358030948980448", "Magic4Life"), MonetaryAmount("0.102", "Magic4Life"));
}

TEST(MonetaryAmountTest, IntegralComparison) {
  EXPECT_EQ(MonetaryAmount("2.00 EUR"), 2);
  EXPECT_EQ(-4, MonetaryAmount("-4.0000 EUR"));

  EXPECT_NE(MonetaryAmount("2.03 EUR"), 2);
  EXPECT_NE(-4, MonetaryAmount("-3.9991 EUR"));

  EXPECT_LT(MonetaryAmount("-0.5 KRW"), 0);
  EXPECT_LT(65, MonetaryAmount("67.5555 KRW"));

  EXPECT_GT(MonetaryAmount("-4092.3 KRW"), -4093);
  EXPECT_GT(11, MonetaryAmount("5.42"));

  EXPECT_LE(MonetaryAmount("-0.5 KRW"), 0);
  EXPECT_LE(MonetaryAmount("-0.0 KRW"), 0);
  EXPECT_LE(65, MonetaryAmount("67.5555 KRW"));
  EXPECT_LE(67, MonetaryAmount("67 KRW"));

  EXPECT_GE(MonetaryAmount("-4092.3 KRW"), -4093);
  EXPECT_GE(MonetaryAmount("-504.0 KRW"), -504);
  EXPECT_GE(11, MonetaryAmount("5.42"));
  EXPECT_GE(7, MonetaryAmount("7"));
}

TEST(MonetaryAmountTest, OverflowProtectionDecimalPart) {
  // OK to truncate decimal part
  EXPECT_LT(MonetaryAmount("94729475.1434000003456523423654", "EUR") - MonetaryAmount("94729475.1434", "EUR"),
            MonetaryAmount("0.0001", "EUR"));
  EXPECT_EQ(MonetaryAmount("123454562433254326.435324", "EUR"), MonetaryAmount("123454562433254326", "EUR"));

  // Should not accept truncation on integral part
  EXPECT_ANY_THROW(MonetaryAmount("1234545624332543260.435324", "EUR"));
}

TEST(MonetaryAmountTest, OverflowProtectionSum) {
  MonetaryAmount lhs("9472902.80094504728", "BTC");
  MonetaryAmount rhs("8577120.15", "BTC");
  EXPECT_EQ(lhs + rhs, MonetaryAmount("18050022.9509450472", "BTC"));  // last digit should be truncated (no rounding)
  EXPECT_EQ(lhs += rhs, MonetaryAmount("18050022.9509450472", "BTC"));
}

TEST(MonetaryAmountTest, OverflowProtectionSub) {
  MonetaryAmount lhs("-9472902.80094504728", "BTC");
  MonetaryAmount rhs("8577120.15", "BTC");
  EXPECT_EQ(lhs - rhs, MonetaryAmount("-18050022.9509450472", "BTC"));
  EXPECT_EQ(lhs -= rhs, MonetaryAmount("-18050022.9509450472", "BTC"));
}

TEST(MonetaryAmountTest, Multiply) {
  EXPECT_EQ(MonetaryAmount("3.25", CurrencyCode("ETH")) * MonetaryAmount("4.578"),
            MonetaryAmount("14.8785", CurrencyCode("ETH")));
  EXPECT_EQ(MonetaryAmount("79871.9000917457") * MonetaryAmount("-34.141590974"),
            MonetaryAmount("-2726953.66542788469"));
  MonetaryAmount res;
  EXPECT_THROW(res = MonetaryAmount(1, "EUR") * MonetaryAmount(2, "ETH"), exception);
}

TEST(MonetaryAmountTest, OverflowProtectionMultiplication) {
  for (CurrencyCode cur : {CurrencyCode("ETH"), CurrencyCode("Magic4Life")}) {
    EXPECT_EQ(MonetaryAmount("-9472902.80094504728", cur) * 3, MonetaryAmount("-28418708.4028351416", cur));
    EXPECT_EQ(MonetaryAmount("9472902.80094504728", cur) * -42, MonetaryAmount("-397861917.639691974", cur));

    EXPECT_EQ(MonetaryAmount("0.00427734447678", cur) * MonetaryAmount("0.9974"),
              MonetaryAmount("0.00426622338114037", cur));
    EXPECT_EQ(MonetaryAmount("38.0566894350664") * MonetaryAmount("0.00008795", cur),
              MonetaryAmount("0.00334708583581405", cur));
    EXPECT_EQ((-1) * MonetaryAmount("-9223372036854775807", cur), MonetaryAmount("922337203685477580", cur));
    EXPECT_EQ((-1) * MonetaryAmount("-922337203685477580", cur), MonetaryAmount("922337203685477580", cur));
  }
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
  MonetaryAmount res;
  EXPECT_THROW(res = MonetaryAmount("100") / MonetaryAmount("0.00000000000000001"), exception);
  EXPECT_EQ(MonetaryAmount(10) / MonetaryAmount("0.0000000000000001"), MonetaryAmount("100000000000000000"));
  EXPECT_EQ(MonetaryAmount("1000000000 KRW") / MonetaryAmount("922337203685477580 KRW"),
            MonetaryAmount("0.00000000108420217"));
}

TEST(MonetaryAmountTest, OverflowProtectionDivide) {
  for (CurrencyCode cur : {CurrencyCode(), CurrencyCode("ETH")}) {
    EXPECT_EQ(MonetaryAmount("0.00353598978800261", cur) / MonetaryAmount("19.65", cur),
              MonetaryAmount("0.00017994858972023"));
    EXPECT_EQ(MonetaryAmount("0.00000598978800261", cur) / MonetaryAmount("19.65", cur),
              MonetaryAmount("0.00000030482381692"));
    EXPECT_EQ(MonetaryAmount("0.00000598978800261", cur) / 17, MonetaryAmount("0.00000035234047074", cur));
  }

  EXPECT_EQ(MonetaryAmount("0.003535989788002", "Magic4Life") / MonetaryAmount("19.65", "Magic4Life"),
            MonetaryAmount("0.0001799485897202"));
  EXPECT_EQ(MonetaryAmount("0.00000598978800261", "Magic4Life") / MonetaryAmount("19.65", "Magic4Life"),
            MonetaryAmount("0.00000030482381689"));
  EXPECT_EQ(MonetaryAmount("0.00000598978800261", "Magic4Life") / 17,
            MonetaryAmount("0.00000035234047074", "Magic4Life"));
}

TEST(MonetaryAmountTest, Convert) {
  EXPECT_EQ(MonetaryAmount(2, "ETH").convertTo(MonetaryAmount("1600", "EUR")), MonetaryAmount("3200", "EUR"));
  EXPECT_EQ(MonetaryAmount("1500", "EUR").convertTo(MonetaryAmount("0.0005", "ETH")), MonetaryAmount("0.75", "ETH"));
}

TEST(MonetaryAmountTest, StringConstructor) {
  EXPECT_EQ(MonetaryAmount("804.62EUR"), MonetaryAmount("804.62", "EUR"));
  EXPECT_EQ(MonetaryAmount("-210.50 CAKE"), MonetaryAmount("-210.50", "CAKE"));
  EXPECT_EQ(MonetaryAmount("05AUD"), MonetaryAmount(5, "AUD"));
  EXPECT_EQ(MonetaryAmount("746REPV2"), MonetaryAmount("746", "REPV2"));

  EXPECT_THROW(MonetaryAmount("usdt"), invalid_argument);
  EXPECT_NO_THROW(MonetaryAmount("usdt", MonetaryAmount::IfNoAmount::kNoThrow));
}

TEST(MonetaryAmountTest, StringConstructorAmbiguity) {
  EXPECT_EQ(MonetaryAmount("804.621INCH"), MonetaryAmount("804.621", "INCH"));
  EXPECT_EQ(MonetaryAmount("804.62 1INCH"), MonetaryAmount("804.62", "1INCH"));
  EXPECT_EQ(MonetaryAmount("804.62", "1INCH"), MonetaryAmount("804.62", "1INCH"));
}

TEST(MonetaryAmountTest, CurrencyTooLong) {
  EXPECT_THROW(MonetaryAmount("804.62 thiscuristoolong"), exception);
  EXPECT_THROW(MonetaryAmount("-210.50magicNumber"), exception);
}

TEST(MonetaryAmountTest, Zero) {
  EXPECT_EQ(MonetaryAmount("0EUR"), 0);
  EXPECT_NE(MonetaryAmount("0.0001EUR"), 0);
}

TEST(MonetaryAmountTest, RoundingPositiveDown) {
  for (CurrencyCode cur : {CurrencyCode("EUR"), CurrencyCode("MAGIC4LIFE")}) {
    MonetaryAmount ma("12.35", cur);
    ma.round(1, MonetaryAmount::RoundType::kDown);
    EXPECT_EQ(ma, MonetaryAmount("12.3", cur));

    ma = MonetaryAmount("12.354", cur);
    ma.round(1, MonetaryAmount::RoundType::kDown);
    EXPECT_EQ(ma, MonetaryAmount("12.3", cur));
  }
}

TEST(MonetaryAmountTest, StepRoundingPositiveDown) {
  MonetaryAmount ma("12.35 EUR");
  ma.round(MonetaryAmount("0.1"), MonetaryAmount::RoundType::kDown);
  EXPECT_EQ(ma, MonetaryAmount("12.3 EUR"));

  ma = MonetaryAmount("12.354 EUR");
  ma.round(MonetaryAmount("0.03"), MonetaryAmount::RoundType::kDown);
  EXPECT_EQ(ma, MonetaryAmount("12.33 EUR"));
}

TEST(MonetaryAmountTest, RoundingPositiveUp) {
  MonetaryAmount ma("12.35 EUR");
  ma.round(1, MonetaryAmount::RoundType::kUp);
  EXPECT_EQ(ma, MonetaryAmount("12.4 EUR"));

  ma = MonetaryAmount("927.4791 EUR");
  ma.round(3, MonetaryAmount::RoundType::kUp);
  EXPECT_EQ(ma, MonetaryAmount("927.48 EUR"));

  ma = MonetaryAmount("12.354 EUR");
  ma.round(1, MonetaryAmount::RoundType::kUp);
  EXPECT_EQ(ma, MonetaryAmount("12.4 EUR"));
}

TEST(MonetaryAmountTest, StepRoundingPositiveUp) {
  MonetaryAmount ma("12.35 EUR");
  ma.round(MonetaryAmount("0.1"), MonetaryAmount::RoundType::kUp);
  EXPECT_EQ(ma, MonetaryAmount("12.4 EUR"));

  ma = MonetaryAmount("12.354 EUR");
  ma.round(MonetaryAmount("1.1"), MonetaryAmount::RoundType::kUp);
  EXPECT_EQ(ma, MonetaryAmount("13.2 EUR"));
}

TEST(MonetaryAmountTest, RoundingPositiveNearest) {
  MonetaryAmount ma("12.307 EUR");
  ma.round(1, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("12.3 EUR"));

  ma = MonetaryAmount("12.34 EUR");
  ma.round(1, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("12.3 EUR"));

  ma = MonetaryAmount("12.58 EUR");
  ma.round(1, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("12.6 EUR"));

  ma = MonetaryAmount("12.5 EUR");
  ma.round(0, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("13 EUR"));

  ma = MonetaryAmount("12.567 EUR");
  ma.round(2, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("12.57 EUR"));

  ma = MonetaryAmount("2899.80000000000018 EUR");
  ma.round(2, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("2899.8 EUR"));

  ma = MonetaryAmount("2400.4 EUR");
  ma.round(2, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("2400.4 EUR"));

  ma = MonetaryAmount("2400.45 EUR");
  ma.round(2, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("2400.45 EUR"));

  ma = MonetaryAmount("2400.45 EUR");
  ma.round(0, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("2400 EUR"));

  ma = MonetaryAmount("2400.51001 EUR");
  ma.round(0, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("2401 EUR"));
}

TEST(MonetaryAmountTest, StepRoundingPositiveNearest) {
  MonetaryAmount ma("12.307 EUR");
  ma.round(MonetaryAmount("0.1"), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("12.3 EUR"));

  ma = MonetaryAmount("12.34 EUR");
  ma.round(MonetaryAmount("0.1"), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("12.3 EUR"));

  ma = MonetaryAmount("12.58 EUR");
  ma.round(MonetaryAmount("0.1"), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("12.6 EUR"));

  ma = MonetaryAmount("12.5 EUR");
  ma.round(MonetaryAmount(1), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("13 EUR"));

  ma = MonetaryAmount("12.5 EUR");
  ma.round(MonetaryAmount("0.5"), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("12.5 EUR"));

  ma = MonetaryAmount("2899.80000000000018 EUR");
  ma.round(MonetaryAmount("0.01"), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("2899.8 EUR"));

  ma = MonetaryAmount("2400.45 EUR");
  ma.round(MonetaryAmount("0.02"), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("2400.46 EUR"));
}

TEST(MonetaryAmountTest, RoundingNegativeDown) {
  MonetaryAmount ma("-23.5 EUR");
  ma.round(0, MonetaryAmount::RoundType::kDown);
  EXPECT_EQ(ma, MonetaryAmount("-24 EUR"));

  ma = MonetaryAmount("-23.51 EUR");
  ma.round(1, MonetaryAmount::RoundType::kDown);
  EXPECT_EQ(ma, MonetaryAmount("-23.6 EUR"));

  ma = MonetaryAmount("-23.51003 EUR");
  ma.round(2, MonetaryAmount::RoundType::kDown);
  EXPECT_EQ(ma, MonetaryAmount("-23.52 EUR"));
}

TEST(MonetaryAmountTest, StepRoundingNegativeDown) {
  MonetaryAmount ma("-23.5 EUR");
  ma.round(MonetaryAmount("0.5"), MonetaryAmount::RoundType::kDown);
  EXPECT_EQ(ma, MonetaryAmount("-23.5 EUR"));

  ma = MonetaryAmount("-23.5 EUR");
  ma.round(MonetaryAmount(1), MonetaryAmount::RoundType::kDown);
  EXPECT_EQ(ma, MonetaryAmount("-24 EUR"));

  ma = MonetaryAmount("-23.50808 EUR");
  ma.round(MonetaryAmount("0.07"), MonetaryAmount::RoundType::kDown);
  EXPECT_EQ(ma, MonetaryAmount("-23.52 EUR"));
}

TEST(MonetaryAmountTest, RoundingNegativeUp) {
  MonetaryAmount ma("-927.4791 EUR");
  ma.round(3, MonetaryAmount::RoundType::kUp);
  EXPECT_EQ(ma, MonetaryAmount("-927.479 EUR"));

  ma = MonetaryAmount("-927.4701 EUR");
  ma.round(3, MonetaryAmount::RoundType::kUp);
  EXPECT_EQ(ma, MonetaryAmount("-927.47 EUR"));

  ma = MonetaryAmount("-927.4701971452 EUR");
  ma.round(6, MonetaryAmount::RoundType::kUp);
  EXPECT_EQ(ma, MonetaryAmount("-927.470197 EUR"));
}

TEST(MonetaryAmountTest, StepRoundingNegativeUp) {
  MonetaryAmount ma("-927.47 EUR");
  ma.round(MonetaryAmount("0.007"), MonetaryAmount::RoundType::kUp);
  EXPECT_EQ(ma, MonetaryAmount("-927.465 EUR"));

  ma = MonetaryAmount("-927.4701971452 EUR");
  ma.round(MonetaryAmount("0.007"), MonetaryAmount::RoundType::kUp);
  EXPECT_EQ(ma, MonetaryAmount("-927.465 EUR"));
}

TEST(MonetaryAmountTest, RoundingNegativeNearest) {
  MonetaryAmount ma("-23.5 EUR");
  ma.round(1, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23.5 EUR"));

  ma = MonetaryAmount("-23.5 EUR");
  ma.round(0, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23 EUR"));

  ma = MonetaryAmount("-23.6 EUR");
  ma.round(0, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-24 EUR"));

  ma = MonetaryAmount("-23.1 EUR");
  ma.round(0, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23 EUR"));

  ma = MonetaryAmount("-23.02099 EUR");
  ma.round(2, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23.02 EUR"));

  ma = MonetaryAmount("-23.02050 EUR");
  ma.round(3, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23.02 EUR"));

  ma = MonetaryAmount("-2400.4 EUR");
  ma.round(2, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-2400.4 EUR"));

  ma = MonetaryAmount("-2400.45 EUR");
  ma.round(2, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-2400.45 EUR"));

  ma = MonetaryAmount("-2400.4784 EUR");
  ma.round(0, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-2400 EUR"));

  ma = MonetaryAmount("-2400.510004 EUR");
  ma.round(0, MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-2401 EUR"));
}

TEST(MonetaryAmountTest, StepRoundingNegativeNearest) {
  MonetaryAmount ma("-23.5 EUR");
  ma.round(MonetaryAmount("0.1"), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23.5 EUR"));

  ma = MonetaryAmount("-23.5 EUR");
  ma.round(MonetaryAmount(1), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23 EUR"));

  ma = MonetaryAmount("-23.6 EUR");
  ma.round(MonetaryAmount(1), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-24 EUR"));

  ma = MonetaryAmount("-23.1 EUR");
  ma.round(MonetaryAmount(1), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23 EUR"));

  ma = MonetaryAmount("-23.02099 EUR");
  ma.round(MonetaryAmount("0.01"), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23.02 EUR"));

  ma = MonetaryAmount("-23.02050 EUR");
  ma.round(MonetaryAmount("0.001"), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23.02 EUR"));

  ma = MonetaryAmount("-23.025500054441 EUR");
  ma.round(MonetaryAmount("0.001"), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(ma, MonetaryAmount("-23.026 EUR"));
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

TEST(MonetaryAmountTest, CloseDoubles) {
  EXPECT_LT(MonetaryAmount(3005.71), MonetaryAmount(3005.72));
  EXPECT_LT(MonetaryAmount(3069.96), MonetaryAmount(3069.97));
  EXPECT_LT(MonetaryAmount(3076.21), MonetaryAmount(3076.22));
  EXPECT_LT(MonetaryAmount(3081.94), MonetaryAmount(3081.95));
}

TEST(MonetaryAmountTest, DoubleWithExpectedPrecision) {
  CurrencyCode cur;
  EXPECT_EQ(MonetaryAmount(3005.71, cur, MonetaryAmount::RoundType::kNearest, 1), MonetaryAmount("3005.7"));
  EXPECT_EQ(MonetaryAmount(-0.0000554, cur, MonetaryAmount::RoundType::kNearest, 5), MonetaryAmount("-0.00006"));
}

TEST(MonetaryAmountTest, Truncate) {
  MonetaryAmount ma("0.00008244");
  ma.truncate(6);
  EXPECT_EQ(ma, MonetaryAmount("0.000082"));
  ma.truncate(4);
  EXPECT_EQ(ma, MonetaryAmount());
  EXPECT_EQ(0, ma);

  ma = MonetaryAmount(std::numeric_limits<MonetaryAmount::AmountType>::max(), CurrencyCode(), 18);
  ma.round(MonetaryAmount(1, CurrencyCode(), 4), MonetaryAmount::RoundType::kNearest);
  EXPECT_EQ(
      ma, MonetaryAmount(std::numeric_limits<MonetaryAmount::AmountType>::max() / ipow10(14) + 1L, CurrencyCode(), 4));
  ma = MonetaryAmount(std::numeric_limits<MonetaryAmount::AmountType>::min(), CurrencyCode(), 18);
  ma.round(MonetaryAmount(1, CurrencyCode(), 4), MonetaryAmount::RoundType::kDown);
  EXPECT_EQ(
      ma, MonetaryAmount(std::numeric_limits<MonetaryAmount::AmountType>::min() / ipow10(14) - 1L, CurrencyCode(), 4));
}

TEST(MonetaryAmountTest, PositiveAmountStr) {
  EXPECT_EQ(MonetaryAmount(7).amountStr(), "7");
  EXPECT_EQ(MonetaryAmount("9204.1260").amountStr(), "9204.126");
  EXPECT_EQ(MonetaryAmount("0.709").amountStr(), "0.709");
  EXPECT_EQ(MonetaryAmount("0.0").amountStr(), "0");
  EXPECT_EQ(MonetaryAmount("3.4950EUR").amountStr(), "3.495");
  EXPECT_EQ(MonetaryAmount("94.5").amountStr(), "94.5");
  EXPECT_EQ(MonetaryAmount("15003.74").amountStr(), "15003.74");
  EXPECT_EQ(MonetaryAmount("15003.740 1INCH").amountStr(), "15003.74");
  EXPECT_EQ(MonetaryAmount("0 KRW").amountStr(), "0");
  EXPECT_EQ(MonetaryAmount("22337203685477.5808 MAGIC4LIFE").amountStr(), "22337203685477.5808");
  EXPECT_EQ(MonetaryAmount("0.000001573004009 MAGIC4LIFE").amountStr(), "0.000001573004009");
  EXPECT_EQ(MonetaryAmount("764.00000000000001 MAGIC4LIFE").amountStr(), "764.00000000000001");
}

TEST(MonetaryAmountTest, NegativeAmountStr) {
  EXPECT_EQ(MonetaryAmount("-3.4950EUR").amountStr(), "-3.495");
  EXPECT_EQ(MonetaryAmount("-0.0034090").amountStr(), "-0.003409");
  EXPECT_EQ(MonetaryAmount("-0.0").amountStr(), "0");
  EXPECT_EQ(MonetaryAmount("-3.4950EUR").amountStr(), "-3.495");
  EXPECT_EQ(MonetaryAmount("-94.5").amountStr(), "-94.5");
  EXPECT_EQ(MonetaryAmount("-15003.740").amountStr(), "-15003.74");
  EXPECT_EQ(MonetaryAmount("-15003.740 1INCH").amountStr(), "-15003.74");
  EXPECT_EQ(MonetaryAmount("-0 KRW").amountStr(), "0");
  EXPECT_EQ(MonetaryAmount("-22337203685477.5808 MAGIC4LIFE").amountStr(), "-22337203685477.5808");
  EXPECT_EQ(MonetaryAmount("-0.000001573004009 MAGIC4LIFE").amountStr(), "-0.000001573004009");
  EXPECT_EQ(MonetaryAmount("-764.00000000000001 MAGIC4LIFE").amountStr(), "-764.00000000000001");
}

TEST(MonetaryAmountTest, AppendAmountStr) {
  {
    string str("");
    MonetaryAmount().appendAmountStr(str);

    EXPECT_EQ("0", str);
  }
  {
    string str("init");
    MonetaryAmount().appendAmountStr(str);

    EXPECT_EQ("init0", str);
  }
  {
    string str("init");
    MonetaryAmount("0a").appendAmountStr(str);

    EXPECT_EQ("init0", str);
  }
  {
    string str("init2");
    MonetaryAmount("67").appendAmountStr(str);

    EXPECT_EQ("init267", str);
  }
  {
    string str("1begin");
    MonetaryAmount("34.56 EUR").appendAmountStr(str);

    EXPECT_EQ("1begin34.56", str);
  }
}

TEST(MonetaryAmountTest, AppendString) {
  {
    string str("");
    MonetaryAmount().appendStrTo(str);

    EXPECT_EQ("0", str);
  }
  {
    string str("init");
    MonetaryAmount().appendStrTo(str);

    EXPECT_EQ("init0", str);
  }
  {
    string str("init");
    MonetaryAmount("0a").appendStrTo(str);

    EXPECT_EQ("init0 A", str);
  }
  {
    string str("init2");
    MonetaryAmount("67").appendStrTo(str);

    EXPECT_EQ("init267", str);
  }
  {
    string str("1begin");
    MonetaryAmount("34.56 EUR").appendStrTo(str);

    EXPECT_EQ("1begin34.56 EUR", str);
  }
}

TEST(MonetaryAmountTest, PositiveStringRepresentation) {
  EXPECT_EQ(MonetaryAmount("3.4950EUR").str(), "3.495 EUR");
  EXPECT_EQ(MonetaryAmount("94.5").str(), "94.5");
  EXPECT_EQ(MonetaryAmount("15003.740 1INCH").str(), "15003.74 1INCH");
  EXPECT_EQ(MonetaryAmount("0 KRW").str(), "0 KRW");
  EXPECT_EQ(MonetaryAmount("22337203685477.5808 MAGIC4LIFE").str(), "22337203685477.5808 MAGIC4LIFE");
  EXPECT_EQ(MonetaryAmount("0.000001573004009 MAGIC4LIFE").str(), "0.000001573004009 MAGIC4LIFE");
  EXPECT_EQ(MonetaryAmount("764.00000000000001 MAGIC4LIFE").str(), "764.00000000000001 MAGIC4LIFE");
}

TEST(MonetaryAmountTest, NegativeStringRepresentation) {
  EXPECT_EQ(MonetaryAmount("-3.4950EUR").str(), "-3.495 EUR");
  EXPECT_EQ(MonetaryAmount("-94.5").str(), "-94.5");
  EXPECT_EQ(MonetaryAmount("-15003.740 1INCH").str(), "-15003.74 1INCH");
  EXPECT_EQ(MonetaryAmount("-0 KRW").str(), "0 KRW");
  EXPECT_EQ(MonetaryAmount("-22337203685477.5808 MAGIC4LIFE").str(), "-22337203685477.5808 MAGIC4LIFE");
  EXPECT_EQ(MonetaryAmount("-0.000001573004009 MAGIC4LIFE").str(), "-0.000001573004009 MAGIC4LIFE");
  EXPECT_EQ(MonetaryAmount("-764.00000000000001 MAGIC4LIFE").str(), "-764.00000000000001 MAGIC4LIFE");
}

TEST(MonetaryAmountTest, ExoticInput) {
  EXPECT_EQ(MonetaryAmount(" +4.6   EUr "), MonetaryAmount("4.6EUR"));
  EXPECT_EQ(MonetaryAmount(" +4.6 ", "EUr"), MonetaryAmount("4.6EUR"));

  // Below ones are needed for Bithumb ('+ 5' for example)
  EXPECT_EQ(MonetaryAmount("+ 4.6   EUr "), MonetaryAmount("4.6EUR"));
  EXPECT_EQ(MonetaryAmount("+ 4.6 ", "EUr"), MonetaryAmount("4.6EUR"));
  EXPECT_EQ(MonetaryAmount("- 0.54 krw "), MonetaryAmount("-0.54", "KRW"));

  EXPECT_EQ(MonetaryAmount(" -.9   f&g "), MonetaryAmount("-0.9F&G"));
  EXPECT_EQ(MonetaryAmount(" -.9", "f&g"), MonetaryAmount("-0.9F&G"));

  EXPECT_EQ(MonetaryAmount(" - .9   f&g "), MonetaryAmount("-0.9F&G"));
  EXPECT_EQ(MonetaryAmount(" - .9", "f&g"), MonetaryAmount("-0.9F&G"));

  EXPECT_EQ(MonetaryAmount(" -.9   f&g "), MonetaryAmount("-0.9F&G"));

  EXPECT_THROW(MonetaryAmount("--.9"), exception);
}

TEST(MonetaryAmountTest, CloseTo) {
  EXPECT_TRUE(MonetaryAmount(1000).isCloseTo(MonetaryAmount(1001), 0.01));
  EXPECT_FALSE(MonetaryAmount(1000).isCloseTo(MonetaryAmount(1001), 0.001));
  EXPECT_TRUE(MonetaryAmount(250).isCloseTo(MonetaryAmount("250.01"), 0.0001));
  EXPECT_TRUE(MonetaryAmount("-3.4").isCloseTo(MonetaryAmount("-3.40001"), 0.0001));
  EXPECT_FALSE(MonetaryAmount("-3.4").isCloseTo(MonetaryAmount("-3.40001"), 0.000001));
  EXPECT_TRUE(MonetaryAmount("-0.90005").isCloseTo(MonetaryAmount("-0.90003"), 0.0001));
  EXPECT_FALSE(MonetaryAmount("-0.90005").isCloseTo(MonetaryAmount("-0.90003"), 0.00001));
  EXPECT_TRUE(MonetaryAmount("-0.90005").isCloseTo(MonetaryAmount("-0.9008"), 0.001));
  EXPECT_FALSE(MonetaryAmount("-0.90005").isCloseTo(MonetaryAmount("-0.9008"), 0.0001));
}

}  // namespace cct