#include <gtest/gtest.h>

#include "apikeysprovider.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "huobiprivateapi.hpp"
#include "huobipublicapi.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {
class HuobiAPI : public ::testing::Test {
 protected:
  HuobiAPI() : huobiPublic(coincenterInfo, fiatConverter, cryptowatchAPI) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  APIKeysProvider apiKeyProvider;
  FiatConverter fiatConverter;
  CryptowatchAPI cryptowatchAPI;
  HuobiPublic huobiPublic;
};

namespace {
void PublicTest(HuobiPublic &huobiPublic) {
  EXPECT_NO_THROW(huobiPublic.queryOrderBook(Market("BTC", "USDT")));
  EXPECT_GT(huobiPublic.queryAllApproximatedOrderBooks().size(), 20U);
  ExchangePublic::WithdrawalFeeMap withdrawFees = huobiPublic.queryWithdrawalFees();
  EXPECT_FALSE(withdrawFees.empty());
  for (const CurrencyExchange &curEx : huobiPublic.queryTradableCurrencies()) {
    EXPECT_TRUE(withdrawFees.contains(curEx.standardCode()));
  }
  ExchangePublic::MarketSet markets = huobiPublic.queryTradableMarkets();
  EXPECT_NO_THROW(huobiPublic.queryLast24hVolume(markets.front()));
}

void PrivateTest(HuobiPrivate &huobiPrivate) {
  // We cannot expect anything from the balance, it may be empty and this is a valid response.
  EXPECT_NO_THROW(huobiPrivate.queryAccountBalance());
  EXPECT_TRUE(huobiPrivate.queryDepositWallet("XRP").hasDestinationTag());
  TradeOptions tradeOptions(TradeStrategy::kMaker, TradeMode::kSimulation, std::chrono::seconds(15));
  MonetaryAmount smallFrom("0.1ETH");
  EXPECT_NO_THROW(huobiPrivate.trade(smallFrom, "BTC", tradeOptions));
  EXPECT_EQ(smallFrom, MonetaryAmount("0ETH"));
}
}  // namespace

/// Place all in the same process to avoid double queries in the public API
TEST_F(HuobiAPI, Main) {
  PublicTest(huobiPublic);

  constexpr char exchangeName[] = "huobi";
  if (!apiKeyProvider.contains(exchangeName)) {
    std::cerr << "Skip Huobi private API test as cannot find associated private key" << std::endl;
    return;
  }

  const APIKey &firstAPIKey =
      apiKeyProvider.get(PrivateExchangeName(exchangeName, apiKeyProvider.getKeyNames(exchangeName).front()));

  HuobiPrivate huobiPrivate(coincenterInfo, huobiPublic, firstAPIKey);

  PrivateTest(huobiPrivate);
}

}  // namespace api
}  // namespace cct