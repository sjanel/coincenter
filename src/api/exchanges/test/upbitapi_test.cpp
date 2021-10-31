#include <gtest/gtest.h>

#include "apikeysprovider.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "tradeoptions.hpp"
#include "upbitprivateapi.hpp"
#include "upbitpublicapi.hpp"

namespace cct {
namespace api {

class UpbitAPI : public ::testing::Test {
 protected:
  UpbitAPI() : upbitPublic(coincenterInfo, fiatConverter, cryptowatchAPI) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  APIKeysProvider apiKeyProvider;
  FiatConverter fiatConverter;
  CryptowatchAPI cryptowatchAPI;
  UpbitPublic upbitPublic;
};

namespace {
void PublicTest(UpbitPublic &upbitPublic) {
  ExchangePublic::MarketSet markets = upbitPublic.queryTradableMarkets();
  CurrencyExchangeFlatSet currencies = upbitPublic.queryTradableCurrencies();

  EXPECT_GT(markets.size(), 10U);
  EXPECT_FALSE(currencies.empty());
  EXPECT_TRUE(std::any_of(currencies.begin(), currencies.end(),
                          [](const CurrencyExchange &currency) { return currency.standardCode().str() == "BTC"; }));
  EXPECT_TRUE(std::any_of(currencies.begin(), currencies.end(),
                          [](const CurrencyExchange &currency) { return currency.standardCode().str() == "KRW"; }));

  ExchangePublic::MarketPriceMap marketPriceMap = upbitPublic.queryAllPrices();
  EXPECT_GT(marketPriceMap.size(), 10U);
  EXPECT_TRUE(marketPriceMap.contains(*markets.begin()));
  EXPECT_TRUE(marketPriceMap.contains(*std::next(markets.begin())));

  ExchangePublic::WithdrawalFeeMap withdrawalFees = upbitPublic.queryWithdrawalFees();
  EXPECT_GT(withdrawalFees.size(), 10U);
  // This unit test makes sure that static snapshot of upbit withdrawal fees looks up to date
  EXPECT_TRUE(withdrawalFees.contains(markets.begin()->base()));
  EXPECT_TRUE(withdrawalFees.contains(std::next(markets.begin(), 1)->base()));
  constexpr CurrencyCode kCurrencyCodesToTest[] = {"BAT", "ETH", "BTC", "XRP"};
  for (CurrencyCode code : kCurrencyCodesToTest) {
    if (currencies.contains(code) && currencies.find(code)->canWithdraw()) {
      EXPECT_FALSE(withdrawalFees.find(code)->second.isZero());
    }
  }

  MarketOrderBook marketOrderBook = upbitPublic.queryOrderBook(*std::next(markets.begin(), 2));
  EXPECT_LT(marketOrderBook.highestBidPrice(), marketOrderBook.lowestAskPrice());
  EXPECT_NO_THROW(upbitPublic.queryLast24hVolume(markets.front()));
  EXPECT_NO_THROW(upbitPublic.queryLastPrice(markets.back()));
}

void PrivateTest(UpbitPrivate &upbitPrivate, UpbitPublic &upbitPublic) {
  // We cannot expect anything from the balance, it may be empty if you are poor and this is a valid response.
  EXPECT_NO_THROW(upbitPrivate.queryAccountBalance());
  EXPECT_TRUE(upbitPrivate.queryDepositWallet("XRP").hasDestinationTag());
  EXPECT_NO_THROW(upbitPrivate.queryTradableCurrencies());
  EXPECT_EQ(upbitPrivate.queryWithdrawalFee("ADA"), upbitPublic.queryWithdrawalFee("ADA"));

  // Uncomment below code to print updated upbit withdrawal fees for static data of withdrawal fees of public API
  // json d;
  // for (const auto &c : upbitPrivate.queryTradableCurrencies()) {
  //   d[string(c.standardStr())] = upbitPrivate.queryWithdrawalFee(c.standardCode()).amountStr();
  // }
  // std::cout << d.dump(2) << std::endl;
}

}  // namespace

TEST_F(UpbitAPI, Public) {
  PublicTest(upbitPublic);

  constexpr char exchangeName[] = "upbit";

  if (!apiKeyProvider.contains(exchangeName)) {
    std::cerr << "Skip Upbit private API test as cannot find associated private key" << std::endl;
    return;
  }

  const APIKey &firstAPIKey =
      apiKeyProvider.get(PrivateExchangeName(exchangeName, apiKeyProvider.getKeyNames(exchangeName).front()));

  // The following test will target the proxy
  // To avoid matching the test case, you can simply provide production keys
  UpbitPrivate upbitPrivate(coincenterInfo, upbitPublic, firstAPIKey);
  PrivateTest(upbitPrivate, upbitPublic);
}

}  // namespace api
}  // namespace cct
