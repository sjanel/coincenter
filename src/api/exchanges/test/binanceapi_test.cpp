#include <gtest/gtest.h>

#include "apikeysprovider.hpp"
#include "binanceprivateapi.hpp"
#include "binancepublicapi.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "commonapi_test.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "tradeoptions.hpp"

namespace cct {
namespace api {
using BinanceAPI = TestAPI<BinancePublic>;

namespace {
ExchangePublic::MarketSet PublicTest(BinancePublic &binancePublic) {
  EXPECT_NO_THROW(binancePublic.queryOrderBook(Market("BTC", "USDT")));
  EXPECT_GT(binancePublic.queryAllApproximatedOrderBooks().size(), 20U);
  ExchangePublic::WithdrawalFeeMap withdrawFees = binancePublic.queryWithdrawalFees();
  EXPECT_FALSE(withdrawFees.empty());
  for (const CurrencyExchange &curEx : binancePublic.queryTradableCurrencies()) {
    EXPECT_TRUE(withdrawFees.contains(curEx.standardCode()));
  }
  ExchangePublic::MarketSet markets = binancePublic.queryTradableMarkets();
  EXPECT_NO_THROW(binancePublic.queryLast24hVolume(markets.front()));
  EXPECT_NO_THROW(binancePublic.queryLastPrice(markets.back()));
  EXPECT_NO_THROW(binancePublic.queryLastTrades(markets.front()));
  return markets;
}

void PrivateTest(BinancePrivate &binancePrivate, BinancePublic &binancePublic,
                 const ExchangePublic::MarketSet &markets) {
  // We cannot expect anything from the balance, it may be empty and this is a valid response.
  EXPECT_NO_THROW(binancePrivate.getAccountBalance());
  auto currencies = binancePrivate.queryTradableCurrencies();
  EXPECT_FALSE(currencies.empty());
  auto foundIt = std::find_if(currencies.begin(), currencies.end(),
                              [](const CurrencyExchange &curExchange) { return curExchange.canDeposit(); });
  if (foundIt != currencies.end()) {
    EXPECT_NO_THROW(binancePrivate.queryDepositWallet(foundIt->standardCode()));
  }
  if (!markets.empty()) {
    Market m = markets.front();
    EXPECT_NO_THROW(binancePrivate.queryOpenedOrders(OpenedOrdersConstraints(m.base(), m.quote())));
  }

  TradeOptions tradeOptions(TradeMode::kSimulation);
  if (currencies.contains(CurrencyCode("BNB"))) {
    MonetaryAmount smallFrom("13.567ADA");
    EXPECT_NO_THROW(binancePrivate.trade(smallFrom, "BNB", tradeOptions));
  }
  MonetaryAmount bigFrom("13567.1234BNB");
  TradedAmounts tradedAmounts = binancePrivate.trade(bigFrom, currencies.back().standardCode(), tradeOptions);
  EXPECT_EQ(tradedAmounts.tradedFrom, MonetaryAmount("13567.1234BNB"));

  EXPECT_EQ(binancePrivate.queryWithdrawalFee(currencies.front().standardCode()),
            binancePublic.queryWithdrawalFee(currencies.front().standardCode()));
}
}  // namespace

/// Place all in the same process to avoid double queries in the public API
TEST_F(BinanceAPI, Main) {
  ExchangePublic::MarketSet markets = PublicTest(exchangePublic);

  constexpr char exchangeName[] = "binance";
  if (!apiKeyProvider.contains(exchangeName)) {
    std::cerr << "Skip Binance private API test as cannot find associated private key" << std::endl;
    return;
  }

  const APIKey &firstAPIKey =
      apiKeyProvider.get(PrivateExchangeName(exchangeName, apiKeyProvider.getKeyNames(exchangeName).front()));

  BinancePrivate binancePrivate(coincenterInfo, exchangePublic, firstAPIKey);

  PrivateTest(binancePrivate, exchangePublic, markets);
}

}  // namespace api
}  // namespace cct