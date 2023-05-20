#include "exchangeinfo.hpp"

#include <gtest/gtest.h>

#include "cct_const.hpp"
#include "durationstring.hpp"
#include "exchangeinfomap.hpp"
#include "exchangeinfoparser.hpp"
#include "loadconfiguration.hpp"

namespace cct {
class ExchangeInfoTest : public ::testing::Test {
 protected:
  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  ExchangeInfoMap exchangeInfoMap{
      ComputeExchangeInfoMap(loadConfiguration.exchangeConfigFileName(), LoadExchangeConfigData(loadConfiguration))};
  ExchangeInfo binanceExchangeInfo{exchangeInfoMap.at("binance")};
  ExchangeInfo bithumbExchangeInfo{exchangeInfoMap.at("bithumb")};
  ExchangeInfo krakenExchangeInfo{exchangeInfoMap.at("kraken")};
};

TEST_F(ExchangeInfoTest, ExcludedAssets) {
  EXPECT_EQ(binanceExchangeInfo.excludedCurrenciesAll(), CurrencyCodeSet({"BQX"}));
  EXPECT_EQ(bithumbExchangeInfo.excludedCurrenciesAll(), CurrencyCodeSet({"AUD", "CAD"}));

  EXPECT_EQ(binanceExchangeInfo.excludedCurrenciesWithdrawal(),
            CurrencyCodeSet({"AUD", "CAD", "CHF", "EUR", "GBP", "JPY", "KRW", "USD"}));

  EXPECT_EQ(krakenExchangeInfo.excludedCurrenciesWithdrawal(),
            CurrencyCodeSet({"AUD", "CAD", "CHF", "EUR", "GBP", "JPY", "KRW", "USD", "KFEE"}));
}

TEST_F(ExchangeInfoTest, TradeFees) {
  EXPECT_EQ(binanceExchangeInfo.applyFee(MonetaryAmount("120.5 ETH"), ExchangeInfo::FeeType::kMaker),
            MonetaryAmount("120.3795 ETH"));
  EXPECT_EQ(binanceExchangeInfo.applyFee(MonetaryAmount("2.356097 ETH"), ExchangeInfo::FeeType::kTaker),
            MonetaryAmount("2.351384806 ETH"));
}

TEST_F(ExchangeInfoTest, Query) {
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(binanceExchangeInfo.publicAPIRate()).count(), 1236);
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(binanceExchangeInfo.privateAPIRate()).count(), 1055);
}

TEST_F(ExchangeInfoTest, MiscellaneousOptions) {
  EXPECT_TRUE(binanceExchangeInfo.multiTradeAllowedByDefault());
  EXPECT_FALSE(binanceExchangeInfo.placeSimulateRealOrder());
  EXPECT_FALSE(binanceExchangeInfo.validateDepositAddressesInFile());
}

}  // namespace cct