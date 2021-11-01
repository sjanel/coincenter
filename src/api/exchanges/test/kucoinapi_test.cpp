#include <gtest/gtest.h>

#include "apikeysprovider.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "kucoinprivateapi.hpp"
#include "kucoinpublicapi.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {
class KucoinAPI : public ::testing::Test {
 protected:
  KucoinAPI()
      : apiKeyProvider(coincenterInfo.dataDir()),
        fiatConverter(coincenterInfo.dataDir()),
        cryptowatchAPI(coincenterInfo),
        kucoinPublic(coincenterInfo, fiatConverter, cryptowatchAPI) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  APIKeysProvider apiKeyProvider;
  FiatConverter fiatConverter;
  CryptowatchAPI cryptowatchAPI;
  KucoinPublic kucoinPublic;
};

namespace {
void PublicTest(KucoinPublic &kucoinPublic) {
  EXPECT_NO_THROW(kucoinPublic.queryOrderBook(Market("BTC", "USDT")));
  EXPECT_GT(kucoinPublic.queryAllApproximatedOrderBooks().size(), 20U);
  ExchangePublic::WithdrawalFeeMap withdrawFees = kucoinPublic.queryWithdrawalFees();
  EXPECT_FALSE(withdrawFees.empty());
  for (const CurrencyExchange &curEx : kucoinPublic.queryTradableCurrencies()) {
    EXPECT_TRUE(withdrawFees.contains(curEx.standardCode()));
  }
  ExchangePublic::MarketSet markets = kucoinPublic.queryTradableMarkets();
  EXPECT_NO_THROW(kucoinPublic.queryLast24hVolume(markets.front()));
  EXPECT_NO_THROW(kucoinPublic.queryLastPrice(markets.back()));
}

void PrivateTest(KucoinPrivate &kucoinPrivate) {
  EXPECT_NO_THROW(kucoinPrivate.queryAccountBalance());
  EXPECT_NO_THROW(kucoinPrivate.queryDepositWallet("XRP"));
  TradeOptions tradeOptions(TradeStrategy::kMaker, TradeMode::kSimulation, std::chrono::seconds(15));
  MonetaryAmount smallFrom("0.1ETH");
  EXPECT_NO_THROW(kucoinPrivate.trade(smallFrom, "BTC", tradeOptions));
  EXPECT_EQ(smallFrom, MonetaryAmount("0ETH"));
}
}  // namespace

/// Place all in the same process to avoid double queries in the public API
TEST_F(KucoinAPI, Main) {
  PublicTest(kucoinPublic);

  constexpr char exchangeName[] = "kucoin";
  if (!apiKeyProvider.contains(exchangeName)) {
    std::cerr << "Skip Kucoin private API test as cannot find associated private key" << std::endl;
    return;
  }

  const APIKey &firstAPIKey =
      apiKeyProvider.get(PrivateExchangeName(exchangeName, apiKeyProvider.getKeyNames(exchangeName).front()));

  KucoinPrivate kucoinPrivate(coincenterInfo, kucoinPublic, firstAPIKey);

  PrivateTest(kucoinPrivate);
}

}  // namespace api
}  // namespace cct