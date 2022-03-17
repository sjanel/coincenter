#include "exchangeinfo.hpp"

#include <gtest/gtest.h>

#include "cct_const.hpp"
#include "durationstring.hpp"
#include "exchangeinfomap.hpp"
#include "exchangeinfoparser.hpp"
#include "loadconfiguration.hpp"

namespace cct {
namespace {
constexpr std::string_view kExchangeNameTest = kSupportedExchanges[0];
}

class ExchangeInfoTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  ExchangeInfoMap exchangeInfoMap{ComputeExchangeInfoMap(LoadExchangeConfigData(loadConfiguration))};
  ExchangeInfo exchangeInfo{exchangeInfoMap.at(kExchangeNameTest)};
};

TEST_F(ExchangeInfoTest, ExcludedAssets) {
  EXPECT_EQ(exchangeInfo.excludedCurrenciesAll(), ExchangeInfo::CurrencySet({"AUD", "CAD"}));
  EXPECT_EQ(exchangeInfo.excludedCurrenciesWithdrawal(), ExchangeInfo::CurrencySet({"BTC", "EUR"}));
}

TEST_F(ExchangeInfoTest, TradeFees) {
  EXPECT_EQ(exchangeInfo.applyFee(MonetaryAmount("120.5 ETH"), ExchangeInfo::FeeType::kMaker),
            MonetaryAmount("120.3795 ETH"));
  EXPECT_EQ(exchangeInfo.applyFee(MonetaryAmount("2.356097 ETH"), ExchangeInfo::FeeType::kTaker),
            MonetaryAmount("2.351384806 ETH"));
}

TEST_F(ExchangeInfoTest, Query) {
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(exchangeInfo.publicAPIRate()).count(), 1236);
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(exchangeInfo.privateAPIRate()).count(), 1055);
}

TEST_F(ExchangeInfoTest, MiscellaneousOptions) {
  EXPECT_TRUE(exchangeInfo.multiTradeAllowedByDefault());
  EXPECT_FALSE(exchangeInfo.placeSimulateRealOrder());
  EXPECT_FALSE(exchangeInfo.validateDepositAddressesInFile());
}

}  // namespace cct