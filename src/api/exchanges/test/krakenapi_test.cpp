#include <gtest/gtest.h>

#include <random>

#include "apikeysprovider.hpp"
#include "cct_proxy.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "krakenprivateapi.hpp"
#include "krakenpublicapi.hpp"
#include "tradeoptionsapi.hpp"

namespace cct {
namespace api {
class KrakenAPI : public ::testing::Test {
 protected:
  KrakenAPI()
      : coincenterProdInfo(settings::kProd),
        coincenterTestInfo(settings::kTest),
        apiProdKeyProvider(coincenterProdInfo.getRunMode()),
        apiTestKeyProvider(coincenterTestInfo.getRunMode()),
        krakenPublic(coincenterProdInfo, fiatConverter, cryptowatchAPI) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterProdInfo;
  CoincenterInfo coincenterTestInfo;
  APIKeysProvider apiProdKeyProvider;
  APIKeysProvider apiTestKeyProvider;
  FiatConverter fiatConverter;
  CryptowatchAPI cryptowatchAPI;
  KrakenPublic krakenPublic;
};

namespace {
void PublicTest(KrakenPublic &krakenPublic) {
  CurrencyExchangeFlatSet currencies = krakenPublic.queryTradableCurrencies();

  EXPECT_GT(currencies.size(), 10U);
  EXPECT_TRUE(std::any_of(currencies.begin(), currencies.end(),
                          [](const CurrencyExchange &c) { return c.standardCode().str() == "BTC"; }));
  EXPECT_TRUE(std::any_of(currencies.begin(), currencies.end(),
                          [](const CurrencyExchange &c) { return c.standardCode().str() == "EUR"; }));

  ExchangePublic::MarketSet markets = krakenPublic.queryTradableMarkets();
  EXPECT_GT(markets.size(), 10);
  const int nbMarkets = markets.size();
  const int kNbOrderBooksToQuery = 4;
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

  ExchangePublic::MarketPriceMap marketPriceMap = krakenPublic.queryAllPrices();
  EXPECT_GT(marketPriceMap.size(), 10);
  EXPECT_TRUE(marketPriceMap.contains(Market("BTC", "EUR")));
  EXPECT_TRUE(marketPriceMap.contains(Market("ETH", "EUR")));

  ExchangePublic::WithdrawalFeeMap withdrawalFees = krakenPublic.queryWithdrawalFees();
  EXPECT_GT(withdrawalFees.size(), 10);
  EXPECT_TRUE(withdrawalFees.contains(CurrencyCode("BTC")));
  EXPECT_TRUE(withdrawalFees.contains(CurrencyCode("ZEC")));
  EXPECT_FALSE(withdrawalFees.find(CurrencyCode("ETH"))->second.isZero());

  EXPECT_GT(krakenPublic.queryAllApproximatedOrderBooks().size(), 10);
}

void PrivateTest(KrakenPrivate &krakenPrivate) {
  // We cannot expect anything from the balance, it may be empty if you are poor and this is a valid response.
  EXPECT_NO_THROW(krakenPrivate.queryAccountBalance());
  TradeOptions tradeOptions(TradeOptions::Strategy::kMaker, TradeOptions::Mode::kSimulation, std::chrono::seconds(15));
  MonetaryAmount smallFrom("0.001BTC");
  EXPECT_NO_THROW(krakenPrivate.trade(smallFrom, "EUR", tradeOptions));
  MonetaryAmount bigFrom("135670067.1234EUR");
  EXPECT_NO_THROW(krakenPrivate.trade(bigFrom, "BTC", tradeOptions));
  EXPECT_LT(bigFrom, MonetaryAmount("13567.1234EUR"));
}
}  // namespace

TEST_F(KrakenAPI, Main) {
  PublicTest(krakenPublic);

  constexpr char exchangeName[] = "kraken";

  if (!apiProdKeyProvider.contains(exchangeName)) {
    std::cerr << "Skip Kraken private API test as cannot find associated private key" << std::endl;
    return;
  }

  const APIKey &firstAPIKey =
      apiProdKeyProvider.get(PrivateExchangeName(exchangeName, apiProdKeyProvider.getKeyNames(exchangeName).front()));

  // The following test will target the proxy
  // To avoid matching the test case, you can simply provide production keys
  KrakenPrivate krakenPrivate(coincenterProdInfo, krakenPublic, firstAPIKey);
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
    KrakenPrivate krakenPrivate(coincenterTestInfo, krakenPublic, firstAPIKey);
    EXPECT_TRUE(krakenPrivate.queryAccountBalance().empty());
  } else {
    log::info("Proxy not available.");
  }
}
}  // namespace api
}  // namespace cct
