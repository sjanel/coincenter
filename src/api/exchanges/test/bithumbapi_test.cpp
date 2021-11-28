#include <gtest/gtest.h>

#include "apikeysprovider.hpp"
#include "bithumbprivateapi.hpp"
#include "bithumbpublicapi.hpp"
#include "coincenterinfo.hpp"
#include "commonapi_test.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {

using BithumbAPI = TestAPI<BithumbPublic>;

namespace {
void PublicTest(BithumbPublic &bithumbPublic) {
  ExchangePublic::MarketSet markets = bithumbPublic.queryTradableMarkets();
  CurrencyExchangeFlatSet currencies = bithumbPublic.queryTradableCurrencies();

  EXPECT_GT(markets.size(), 10U);
  EXPECT_FALSE(currencies.empty());
  EXPECT_TRUE(std::any_of(currencies.begin(), currencies.end(),
                          [](const CurrencyExchange &currency) { return currency.standardCode().str() == "BTC"; }));
  EXPECT_TRUE(std::any_of(currencies.begin(), currencies.end(),
                          [](const CurrencyExchange &currency) { return currency.standardCode().str() == "KRW"; }));

  EXPECT_GT(bithumbPublic.queryAllApproximatedOrderBooks(1).size(), 10U);
  ExchangePublic::MarketPriceMap marketPriceMap = bithumbPublic.queryAllPrices();
  EXPECT_GT(marketPriceMap.size(), 10U);
  EXPECT_TRUE(marketPriceMap.contains(*markets.begin()));
  EXPECT_TRUE(marketPriceMap.contains(*std::next(markets.begin())));

  ExchangePublic::WithdrawalFeeMap withdrawalFees = bithumbPublic.queryWithdrawalFees();
  EXPECT_GT(withdrawalFees.size(), 10U);
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
  EXPECT_NO_THROW(bithumbPublic.queryLast24hVolume(markets.front()));
  EXPECT_NO_THROW(bithumbPublic.queryLastPrice(markets.back()));
}

void PrivateTest(BithumbPrivate &bithumbPrivate) {
  // We cannot expect anything from the balance, it may be empty and this is a valid response.
  EXPECT_NO_THROW(bithumbPrivate.getAccountBalance());
  EXPECT_FALSE(bithumbPrivate.queryDepositWallet("ETH").hasTag());
  TradeOptions tradeOptions(TradeMode::kSimulation);
  MonetaryAmount smallFrom("13.567XRP");
  EXPECT_NO_THROW(bithumbPrivate.trade(smallFrom, "KRW", tradeOptions));
  MonetaryAmount bigFrom("135670067.1234KRW");
  EXPECT_NO_THROW(bithumbPrivate.trade(bigFrom, "ETH", tradeOptions));
  EXPECT_LT(bigFrom, MonetaryAmount("13567.1234KRW"));
}
}  // namespace

TEST_F(BithumbAPI, Public) {
  PublicTest(exchangePublic);

  constexpr char exchangeName[] = "bithumb";
  if (!apiKeyProvider.contains(exchangeName)) {
    std::cerr << "Skip Bithumb private API test as cannot find associated private key" << std::endl;
    return;
  }

  const APIKey &firstAPIKey =
      apiKeyProvider.get(PrivateExchangeName(exchangeName, apiKeyProvider.getKeyNames(exchangeName).front()));

  BithumbPrivate bithumbPrivate(coincenterInfo, exchangePublic, firstAPIKey);

  // We cannot expect anything from the balance, it may be empty and this is a valid response.
  PrivateTest(bithumbPrivate);
}

}  // namespace api
}  // namespace cct
