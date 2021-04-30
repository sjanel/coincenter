#include <gtest/gtest.h>

#include "apikeysprovider.hpp"
#include "binanceprivateapi.hpp"
#include "binancepublicapi.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "tradeoptionsapi.hpp"

namespace cct {
namespace api {
class BinanceAPI : public ::testing::Test {
 protected:
  BinanceAPI() : binancePublic(coincenterInfo, fiatConverter, cryptowatchAPI) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  APIKeysProvider apiKeyProvider;
  FiatConverter fiatConverter;
  CryptowatchAPI cryptowatchAPI;
  BinancePublic binancePublic;
};

namespace {
void PublicTest(BinancePublic &binancePublic) {
  EXPECT_NO_THROW(binancePublic.queryOrderBook(Market("PSG", "BTC")));
  EXPECT_GT(binancePublic.queryAllApproximatedOrderBooks().size(), 20);
  EXPECT_NO_THROW(binancePublic.queryTradableCurrencies());
  EXPECT_NO_THROW(binancePublic.queryWithdrawalFees());
  EXPECT_NO_THROW(binancePublic.queryTradableMarkets());
}

void PrivateTest(BinancePrivate &binancePrivate) {
  // We cannot expect anything from the balance, it may be empty and this is a valid response.
  EXPECT_NO_THROW(binancePrivate.queryAccountBalance());
  TradeOptions tradeOptions(TradeOptions::Strategy::kMaker, TradeOptions::Mode::kSimulation, std::chrono::seconds(15));
  MonetaryAmount smallFrom("13.567ADA");
  EXPECT_NO_THROW(binancePrivate.trade(smallFrom, "BNB", tradeOptions));
  MonetaryAmount bigFrom("13567.1234BNB");
  EXPECT_NO_THROW(binancePrivate.trade(bigFrom, "ADA", tradeOptions));
  EXPECT_LT(bigFrom, MonetaryAmount("13567.1234BNB"));
}
}  // namespace

/// Place all in the same process to avoid double queries in the public API
TEST_F(BinanceAPI, Main) {
  PublicTest(binancePublic);

  constexpr char exchangeName[] = "binance";
  if (!apiKeyProvider.contains(exchangeName)) {
    std::cerr << "Skip Binance private API test as cannot find associated private key" << std::endl;
    return;
  }

  const APIKey &firstAPIKey =
      apiKeyProvider.get(PrivateExchangeName(exchangeName, apiKeyProvider.getKeyNames(exchangeName).front()));

  BinancePrivate binancePrivate(coincenterInfo, binancePublic, firstAPIKey);

  PrivateTest(binancePrivate);
}

}  // namespace api
}  // namespace cct