#include "exchangeconfig.hpp"

#include <gtest/gtest.h>

#include <chrono>

#include "cct_const.hpp"
#include "currencycodeset.hpp"
#include "exchangeconfigmap.hpp"
#include "exchangeconfigparser.hpp"
#include "loadconfiguration.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {
class ExchangeConfigTest : public ::testing::Test {
 protected:
  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  ExchangeConfigMap exchangeConfigMap{
      ComputeExchangeConfigMap(loadConfiguration.exchangeConfigFileName(), LoadExchangeConfigData(loadConfiguration))};
  ExchangeConfig binanceExchangeInfo{exchangeConfigMap.at("binance")};
  ExchangeConfig bithumbExchangeInfo{exchangeConfigMap.at("bithumb")};
  ExchangeConfig krakenExchangeInfo{exchangeConfigMap.at("kraken")};
};

TEST_F(ExchangeConfigTest, ExcludedAssets) {
  EXPECT_EQ(binanceExchangeInfo.excludedCurrenciesAll(), CurrencyCodeSet({"BQX"}));
  EXPECT_EQ(bithumbExchangeInfo.excludedCurrenciesAll(), CurrencyCodeSet({"AUD", "CAD"}));

  EXPECT_EQ(binanceExchangeInfo.excludedCurrenciesWithdrawal(),
            CurrencyCodeSet({"AUD", "CAD", "CHF", "EUR", "GBP", "JPY", "KRW", "USD"}));

  EXPECT_EQ(krakenExchangeInfo.excludedCurrenciesWithdrawal(),
            CurrencyCodeSet({"AUD", "CAD", "CHF", "EUR", "GBP", "JPY", "KRW", "USD", "KFEE"}));
}

TEST_F(ExchangeConfigTest, TradeFees) {
  EXPECT_EQ(binanceExchangeInfo.applyFee(MonetaryAmount("120.5 ETH"), ExchangeConfig::FeeType::kMaker),
            MonetaryAmount("120.3795 ETH"));
  EXPECT_EQ(binanceExchangeInfo.applyFee(MonetaryAmount("2.356097 ETH"), ExchangeConfig::FeeType::kTaker),
            MonetaryAmount("2.351384806 ETH"));
}

TEST_F(ExchangeConfigTest, Query) {
  EXPECT_EQ(std::chrono::duration_cast<milliseconds>(binanceExchangeInfo.publicAPIRate()).count(), 1236);
  EXPECT_EQ(std::chrono::duration_cast<milliseconds>(binanceExchangeInfo.privateAPIRate()).count(), 1055);
}

TEST_F(ExchangeConfigTest, MiscellaneousOptions) {
  EXPECT_TRUE(binanceExchangeInfo.multiTradeAllowedByDefault());
  EXPECT_FALSE(binanceExchangeInfo.placeSimulateRealOrder());
  EXPECT_FALSE(binanceExchangeInfo.validateDepositAddressesInFile());
}

}  // namespace cct
