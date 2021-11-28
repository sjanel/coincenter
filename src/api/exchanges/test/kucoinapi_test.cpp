#include <gtest/gtest.h>

#include "apikeysprovider.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "commonapi_test.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "kucoinprivateapi.hpp"
#include "kucoinpublicapi.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {
using KucoinAPI = TestAPI<KucoinPublic>;

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
  EXPECT_NO_THROW(kucoinPublic.queryLastTrades(markets.front()));
}

void PrivateTest(KucoinPrivate &kucoinPrivate) {
  EXPECT_NO_THROW(kucoinPrivate.getAccountBalance());
  EXPECT_NO_THROW(kucoinPrivate.queryDepositWallet("XRP"));
  TradeOptions tradeOptions(TradeMode::kSimulation);
  MonetaryAmount smallFrom("0.1ETH");
  EXPECT_NO_THROW(kucoinPrivate.trade(smallFrom, "BTC", tradeOptions));
  EXPECT_EQ(smallFrom, MonetaryAmount("0ETH"));
}
}  // namespace

/// Place all in the same process to avoid double queries in the public API
TEST_F(KucoinAPI, Main) {
  PublicTest(exchangePublic);

  constexpr char exchangeName[] = "kucoin";
  if (!apiKeyProvider.contains(exchangeName)) {
    std::cerr << "Skip Kucoin private API test as cannot find associated private key" << std::endl;
    return;
  }

  const APIKey &firstAPIKey =
      apiKeyProvider.get(PrivateExchangeName(exchangeName, apiKeyProvider.getKeyNames(exchangeName).front()));

  KucoinPrivate kucoinPrivate(coincenterInfo, exchangePublic, firstAPIKey);

  PrivateTest(kucoinPrivate);
}

}  // namespace api
}  // namespace cct