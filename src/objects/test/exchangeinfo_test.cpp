#include "exchangeinfo.hpp"

#include <gtest/gtest.h>

namespace cct {
namespace {

const CurrencyCode kExcludedCur[] = {CurrencyCode("AUD"), CurrencyCode("CAD")};
const CurrencyCode kWithdrawExcludedCur[] = {CurrencyCode("BTC"), CurrencyCode("EUR")};
}  // namespace

class ExchangeInfoKrakenTest : public ::testing::Test {
 protected:
  ExchangeInfoKrakenTest()
      : exchangeInfo("kraken", "0.16", "0.26", kExcludedCur, kWithdrawExcludedCur, 666, 450, false) {}
  virtual void SetUp() {}
  virtual void TearDown() {}

  ExchangeInfo exchangeInfo;
};

TEST_F(ExchangeInfoKrakenTest, ExcludedAssets) {
  EXPECT_EQ(exchangeInfo.excludedCurrenciesAll(),
            ExchangeInfo::CurrencySet({CurrencyCode("AUD"), CurrencyCode("CAD")}));
  EXPECT_EQ(exchangeInfo.excludedCurrenciesWithdrawal(),
            ExchangeInfo::CurrencySet({CurrencyCode("BTC"), CurrencyCode("EUR")}));
}

TEST_F(ExchangeInfoKrakenTest, TradeFees) {
  EXPECT_EQ(exchangeInfo.applyFee(MonetaryAmount("120.5 ETH"), ExchangeInfo::FeeType::kMaker),
            MonetaryAmount("120.3072", CurrencyCode("ETH")));
  EXPECT_EQ(exchangeInfo.applyFee(MonetaryAmount("2.356097 ETH"), ExchangeInfo::FeeType::kTaker),
            MonetaryAmount("2.3499711478", CurrencyCode("ETH")));
}

TEST_F(ExchangeInfoKrakenTest, Query) {
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(exchangeInfo.minPublicQueryDelay()).count(), 666);
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(exchangeInfo.minPrivateQueryDelay()).count(), 450);
}

}  // namespace cct