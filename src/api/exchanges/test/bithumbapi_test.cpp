#include <gtest/gtest.h>

#include "apikeysprovider.hpp"
#include "bithumbprivateapi.hpp"
#include "bithumbpublicapi.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {

class BithumbAPI : public ::testing::Test {
 protected:
  BithumbAPI() : bithumbPublic(coincenterInfo, fiatConverter, cryptowatchAPI) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  APIKeysProvider apiKeyProvider;
  FiatConverter fiatConverter;
  CryptowatchAPI cryptowatchAPI;
  BithumbPublic bithumbPublic;
};

namespace {
void PublicTest(BithumbPublic &bithumbPublic) {
  ExchangePublic::MarketSet markets = bithumbPublic.queryTradableMarkets();
  CurrencyExchangeFlatSet currencies = bithumbPublic.queryTradableCurrencies();

  EXPECT_GT(markets.size(), 10);
  EXPECT_FALSE(currencies.empty());
  EXPECT_TRUE(std::any_of(currencies.begin(), currencies.end(),
                          [](const CurrencyExchange &currency) { return currency.standardCode().str() == "BTC"; }));
  EXPECT_TRUE(std::any_of(currencies.begin(), currencies.end(),
                          [](const CurrencyExchange &currency) { return currency.standardCode().str() == "KRW"; }));

  EXPECT_GT(bithumbPublic.queryAllApproximatedOrderBooks(1).size(), 10);
  ExchangePublic::MarketPriceMap marketPriceMap = bithumbPublic.queryAllPrices();
  EXPECT_GT(marketPriceMap.size(), 10);
  EXPECT_TRUE(marketPriceMap.contains(*markets.begin()));
  EXPECT_TRUE(marketPriceMap.contains(*std::next(markets.begin())));

  ExchangePublic::WithdrawalFeeMap withdrawalFees = bithumbPublic.queryWithdrawalFees();
  EXPECT_GT(withdrawalFees.size(), 10);
  EXPECT_TRUE(withdrawalFees.contains(markets.begin()->base()));
  EXPECT_TRUE(withdrawalFees.contains(std::next(markets.begin(), 1)->base()));
  const CurrencyCode kCurrencyCodesToTest[] = {"BAT", "ETH", "BTC", "XRP"};
  for (CurrencyCode code : kCurrencyCodesToTest) {
    if (currencies.contains(code) && currencies.find(code)->canWithdraw()) {
      EXPECT_FALSE(withdrawalFees.find(code)->second.isZero());
    }
  }

  MarketOrderBook marketOrderBook = bithumbPublic.queryOrderBook(*std::next(markets.begin(), 2));
  EXPECT_LT(marketOrderBook.highestBidPrice(), marketOrderBook.lowestAskPrice());
}

void PrivateTest(BithumbPrivate &bithumbPrivate) {
  // We cannot expect anything from the balance, it may be empty and this is a valid response.
  EXPECT_NO_THROW(bithumbPrivate.queryAccountBalance());
  TradeOptions tradeOptions(TradeStrategy::kMaker, TradeMode::kSimulation, std::chrono::seconds(15));
  MonetaryAmount smallFrom("13.567XRP");
  EXPECT_NO_THROW(bithumbPrivate.trade(smallFrom, "KRW", tradeOptions));
  MonetaryAmount bigFrom("135670067.1234KRW");
  EXPECT_NO_THROW(bithumbPrivate.trade(bigFrom, "ETH", tradeOptions));
  EXPECT_LT(bigFrom, MonetaryAmount("13567.1234KRW"));
}
}  // namespace

TEST_F(BithumbAPI, Public) {
  PublicTest(bithumbPublic);

  constexpr char exchangeName[] = "bithumb";
  if (!apiKeyProvider.contains(exchangeName)) {
    std::cerr << "Skip Bithumb private API test as cannot find associated private key" << std::endl;
    return;
  }

  const APIKey &firstAPIKey =
      apiKeyProvider.get(PrivateExchangeName(exchangeName, apiKeyProvider.getKeyNames(exchangeName).front()));

  BithumbPrivate bithumbPrivate(coincenterInfo, bithumbPublic, firstAPIKey);

  // We cannot expect anything from the balance, it may be empty and this is a valid response.
  PrivateTest(bithumbPrivate);
}

}  // namespace api
}  // namespace cct
