#include <gtest/gtest.h>

#include <random>

#include "apikeysprovider.hpp"
#include "cct_proxy.hpp"
#include "coincenterinfo.hpp"
#include "commonapi_test.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "krakenprivateapi.hpp"
#include "krakenpublicapi.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {
using KrakenAPI = TestAPI<KrakenPublic>;

namespace {
void PublicTest(KrakenPublic &krakenPublic) {
  CurrencyExchangeFlatSet currencies = krakenPublic.queryTradableCurrencies();

  EXPECT_GT(currencies.size(), 10U);
  EXPECT_TRUE(std::any_of(currencies.begin(), currencies.end(),
                          [](const CurrencyExchange &c) { return c.standardCode().str() == "BTC"; }));
  EXPECT_TRUE(std::any_of(currencies.begin(), currencies.end(),
                          [](const CurrencyExchange &c) { return c.standardCode().str() == "EUR"; }));

  ExchangePublic::MarketSet markets = krakenPublic.queryTradableMarkets();
  EXPECT_GT(markets.size(), 10U);
  const int nbMarkets = markets.size();
  const int kNbOrderBooksToQuery = 2;
  ExchangePublic::MarketSet sampleMarkets;
  sampleMarkets.reserve(std::min(nbMarkets, kNbOrderBooksToQuery));
  std::sample(markets.begin(), markets.end(), std::inserter(sampleMarkets, sampleMarkets.end()), kNbOrderBooksToQuery,
              std::mt19937{std::random_device{}()});
  const int kCountDepthOrderBook = 3;
  for (Market m : sampleMarkets) {
    MarketOrderBook marketOrderBook = krakenPublic.queryOrderBook(m, kCountDepthOrderBook);
    if (!marketOrderBook.empty()) {
      EXPECT_LT(marketOrderBook.highestBidPrice(), marketOrderBook.lowestAskPrice());
    }
  }

  EXPECT_GT(krakenPublic.queryAllApproximatedOrderBooks(1).size(), 10);
  ExchangePublic::MarketPriceMap marketPriceMap = krakenPublic.queryAllPrices();
  EXPECT_GT(marketPriceMap.size(), 10U);
  EXPECT_TRUE(marketPriceMap.contains(Market("BTC", "EUR")));
  EXPECT_TRUE(marketPriceMap.contains(Market("ETH", "EUR")));

  ExchangePublic::WithdrawalFeeMap withdrawalFees = krakenPublic.queryWithdrawalFees();
  EXPECT_GT(withdrawalFees.size(), 10U);
  EXPECT_TRUE(withdrawalFees.contains(CurrencyCode("BTC")));
  EXPECT_TRUE(withdrawalFees.contains(CurrencyCode("ZEC")));
  EXPECT_FALSE(withdrawalFees.find(CurrencyCode("ETH"))->second.isZero());
  EXPECT_NO_THROW(krakenPublic.queryLast24hVolume(markets.front()));
  EXPECT_NO_THROW(krakenPublic.queryLastPrice(markets.back()));
}

void PrivateTest(KrakenPrivate &krakenPrivate) {
  // We cannot expect anything from the balance, it may be empty if you are poor and this is a valid response.
  EXPECT_NO_THROW(krakenPrivate.queryAccountBalance());
  EXPECT_FALSE(krakenPrivate.queryDepositWallet("BCH").hasDestinationTag());
  TradeOptions tradeOptions(TradeStrategy::kMaker, TradeMode::kSimulation, std::chrono::seconds(15));
  MonetaryAmount smallFrom("0.001BTC");
  EXPECT_NO_THROW(krakenPrivate.trade(smallFrom, "EUR", tradeOptions));
  MonetaryAmount stdFrom("100.1234EUR");
  EXPECT_NO_THROW(krakenPrivate.trade(stdFrom, "BTC", tradeOptions));
  EXPECT_LT(stdFrom, MonetaryAmount("50EUR"));
}
}  // namespace

TEST_F(KrakenAPI, Main) {
  PublicTest(exchangePublic);

  constexpr char exchangeName[] = "kraken";

  if (!apiKeyProvider.contains(exchangeName)) {
    std::cerr << "Skip Kraken private API test as cannot find associated private key" << std::endl;
    return;
  }

  const APIKey &firstAPIKey =
      apiKeyProvider.get(PrivateExchangeName(exchangeName, apiKeyProvider.getKeyNames(exchangeName).front()));

  // The following test will target the proxy
  // To avoid matching the test case, you can simply provide production keys
  KrakenPrivate krakenPrivate(coincenterInfo, exchangePublic, firstAPIKey);
  PrivateTest(krakenPrivate);
}

TEST_F(KrakenAPI, PrivateEmptyBalance) {
  if (IsProxyAvailable()) {
    constexpr char exchangeName[] = "kraken";

    if (!apiTestKeyProvider.contains(exchangeName)) {
      std::cerr << "Skip Kraken private API test as cannot find associated private key" << std::endl;
      return;
    }

    const APIKey &firstAPIKey =
        apiTestKeyProvider.get(PrivateExchangeName(exchangeName, apiTestKeyProvider.getKeyNames(exchangeName).front()));

    // The following test will target the proxy to ensure stable response
    // To avoid matching the test case, you can simply provide production keys
    KrakenPrivate krakenPrivate(coincenterTestInfo, exchangePublic, firstAPIKey);
    EXPECT_TRUE(krakenPrivate.queryAccountBalance().empty());
  } else {
    log::info("Proxy not available.");
  }
}
}  // namespace api
}  // namespace cct
