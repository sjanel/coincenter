#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <random>

#include "apikeysprovider.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"

namespace cct::api {

template <class PublicExchangeT, class PrivateExchangeT>
class TestAPI {
 public:
  TestAPI() { createPrivateExchangeIfKeyPresent(exchangePublic, coincenterInfo, apiKeysProvider); }

  static MarketSet ComputeMarketSetSample(const MarketSet &markets, const CurrencyExchangeFlatSet &currencies) {
    static constexpr int kNbSamples = 1;
    MarketSet consideredMarkets;
    std::ranges::copy_if(markets, std::inserter(consideredMarkets, consideredMarkets.end()), [&currencies](Market mk) {
      auto cur1It = currencies.find(mk.base());
      auto cur2It = currencies.find(mk.quote());
      return cur1It != currencies.end() && cur2It != currencies.end() && (!cur1It->isFiat() || !cur2It->isFiat());
    });
    MarketSet sampleMarkets;
    std::ranges::sample(consideredMarkets, std::inserter(sampleMarkets, sampleMarkets.end()), kNbSamples,
                        std::mt19937{std::random_device{}()});
    return sampleMarkets;
  }

  static CurrencyExchangeFlatSet ComputeCurrencyExchangeSample(const MarketSet &markets,
                                                               const CurrencyExchangeFlatSet &currencies) {
    CurrencyExchangeFlatSet currencyToKeep;
    std::ranges::copy_if(
        currencies, std::inserter(currencyToKeep, currencyToKeep.end()), [&](const CurrencyExchange &c) {
          return !c.isFiat() && std::ranges::any_of(markets, [&c](Market mk) { return mk.canTrade(c.standardCode()); });
        });

    CurrencyExchangeFlatSet sample;
    std::ranges::sample(currencyToKeep, std::inserter(sample, sample.end()), 1, std::mt19937{std::random_device{}()});
    return sample;
  }

  settings::RunMode runMode = settings::RunMode::kProd;
  LoadConfiguration loadConfig{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  CoincenterInfo coincenterInfo{runMode, loadConfig};
  APIKeysProvider apiKeysProvider{coincenterInfo.dataDir(), coincenterInfo.getRunMode()};
  FiatConverter fiatConverter{coincenterInfo, Duration::max()};  // max to avoid real Fiat converter queries
  CommonAPI commonAPI{coincenterInfo, Duration::max()};
  PublicExchangeT exchangePublic{coincenterInfo, fiatConverter, commonAPI};
  std::optional<PrivateExchangeT> exchangePrivateOpt;

  CurrencyExchangeFlatSet currencies;
  MarketSet markets;
  MarketSet sampleMarkets;
  bool exchangeStatusOK = false;

  void testHealthCheck() { exchangeStatusOK = exchangePublic.healthCheck(); }

  void testCurrencies() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    currencies =
        exchangePrivateOpt ? exchangePrivateOpt->queryTradableCurrencies() : exchangePublic.queryTradableCurrencies();
    ASSERT_FALSE(currencies.empty());
    EXPECT_TRUE(
        std::ranges::none_of(currencies, [](const CurrencyExchange &c) { return c.standardCode().str().empty(); }));

    // Uncomment below code to print updated Upbit withdrawal fees for static data of withdrawal fees of public API
    // if (exchangePrivateOpt) {
    //   json d;
    //   for (const auto &c : currencies) {
    //     d[string(c.standardStr())] = exchangePrivateOpt->queryWithdrawalFee(c.standardCode()).amountStr();
    //   }
    //   std::cout << d.dump(2) << '\n';
    // }
  }

  void testMarkets() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    markets = exchangePublic.queryTradableMarkets();
    sampleMarkets = ComputeMarketSetSample(markets, currencies);
    for (Market mk : sampleMarkets) {
      testMarket(mk);
    }
  }

  void testMarket(Market mk) {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    log::info("Test {} market", mk);
    ASSERT_FALSE(markets.empty());
    static constexpr int kCountDepthOrderBook = 5;
    MarketOrderBook marketOrderBook = exchangePublic.queryOrderBook(mk, kCountDepthOrderBook);
    EXPECT_LE(marketOrderBook.nbAskPrices(), kCountDepthOrderBook);
    EXPECT_LE(marketOrderBook.nbBidPrices(), kCountDepthOrderBook);
    EXPECT_FALSE(marketOrderBook.isArtificiallyExtended());
    if (!marketOrderBook.empty()) {
      EXPECT_LT(marketOrderBook.highestBidPrice(), marketOrderBook.lowestAskPrice());
    }
    EXPECT_NO_THROW(exchangePublic.queryLast24hVolume(mk));
    EXPECT_NO_THROW(exchangePublic.queryLastPrice(mk));

    MarketOrderBookMap approximatedMarketOrderBooks = exchangePublic.queryAllApproximatedOrderBooks(1);

    auto approximatedOrderbookIt = approximatedMarketOrderBooks.find(mk);
    ASSERT_NE(approximatedOrderbookIt, approximatedMarketOrderBooks.end());

    MarketPriceMap marketPriceMap = exchangePublic.queryAllPrices();

    auto marketPriceIt = marketPriceMap.find(mk);
    ASSERT_NE(marketPriceIt, marketPriceMap.end());
  }

  void testWithdrawalFees() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    CurrencyExchangeFlatSet withdrawableCryptos;
    std::ranges::copy_if(
        currencies, std::inserter(withdrawableCryptos, withdrawableCryptos.end()), [this](const CurrencyExchange &c) {
          return !c.isFiat() && c.canWithdraw() &&
                 std::ranges::any_of(markets, [&c](Market mk) { return mk.canTrade(c.standardCode()); });
        });

    if (!withdrawableCryptos.empty()) {
      CurrencyExchangeFlatSet sample;
      if (exchangePublic.isWithdrawalFeesSourceReliable()) {
        std::ranges::sample(withdrawableCryptos, std::inserter(sample, sample.end()), 1,
                            std::mt19937{std::random_device{}()});
      } else {
        // If exchange withdrawal fees source is not reliable, make several tries
        sample = std::move(withdrawableCryptos);
      }

      WithdrawalFeeMap withdrawalFees =
          exchangePrivateOpt ? exchangePrivateOpt->queryWithdrawalFees() : exchangePublic.queryWithdrawalFees();

      for (const CurrencyExchange &curExchange : sample) {
        CurrencyCode cur(curExchange.standardCode());
        log::info("Choosing {} as random currency code for Withdrawal fee test", cur);
        auto withdrawalFeeIt = withdrawalFees.find(cur);
        if (exchangePublic.isWithdrawalFeesSourceReliable() || withdrawalFeeIt != withdrawalFees.end()) {
          ASSERT_NE(withdrawalFeeIt, withdrawalFees.end());
          EXPECT_GE(withdrawalFeeIt->second, MonetaryAmount(0, withdrawalFeeIt->second.currencyCode()));
          break;
        } else {
          log::warn("{} withdrawal fee is not known (unreliable source), trying another one", cur);
        }
      }
    }
  }

  void testBalance() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    if (exchangePrivateOpt) {
      EXPECT_NO_THROW(exchangePrivateOpt->getAccountBalance());
    }
  }

  void testDepositWallet() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    if (exchangePrivateOpt) {
      CurrencyExchangeFlatSet depositableCryptos;
      std::ranges::copy_if(
          currencies, std::inserter(depositableCryptos, depositableCryptos.end()), [this](const CurrencyExchange &c) {
            return !c.isFiat() && c.canDeposit() &&
                   std::ranges::any_of(markets, [&c](Market mk) { return mk.canTrade(c.standardCode()); });
          });
      if (!depositableCryptos.empty()) {
        CurrencyExchangeFlatSet sample;
        int nbSamples = exchangePrivateOpt->canGenerateDepositAddress() ? 1 : 5;
        std::ranges::sample(depositableCryptos, std::inserter(sample, sample.end()), nbSamples,
                            std::mt19937{std::random_device{}()});

        for (const CurrencyExchange &curExchange : sample) {
          CurrencyCode cur(curExchange.standardCode());
          log::info("Choosing {} as random currency code for Deposit wallet test", cur);
          try {
            Wallet wallet = exchangePrivateOpt->queryDepositWallet(cur);
            EXPECT_FALSE(wallet.address().empty());
            break;
          } catch (const exception &) {
            if (exchangePrivateOpt->canGenerateDepositAddress()) {
              throw;
            } else {
              log::info("Wallet for {} is not generated, taking next one", cur);
            }
          }
        }
      }
    }
  }

  void testOpenedOrders() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    if (exchangePrivateOpt && !sampleMarkets.empty()) {
      Market mk = sampleMarkets.front();
      Orders baseOpenedOrders = exchangePrivateOpt->queryOpenedOrders(OrdersConstraints(mk.base()));
      if (!baseOpenedOrders.empty()) {
        const Order &openedOrder = baseOpenedOrders.front();
        EXPECT_TRUE(openedOrder.market().canTrade(mk.base()));
        EXPECT_LT(openedOrder.matchedVolume(), openedOrder.originalVolume());
      }
    }
  }

  void testRecentDeposits() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    if (exchangePrivateOpt) {
      for (const CurrencyExchange &curExchange : ComputeCurrencyExchangeSample(markets, currencies)) {
        CurrencyCode cur(curExchange.standardCode());
        log::info("Choosing {} as random currency code for Recent deposits test", cur);
        DepositsSet deposits = exchangePrivateOpt->queryRecentDeposits(DepositsConstraints(cur));
        if (!deposits.empty()) {
          EXPECT_EQ(deposits.front().amount().currencyCode(), cur);
        }
      }
    }
  }

  void testRecentWithdraws() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    if (exchangePrivateOpt) {
      for (const CurrencyExchange &curExchange : ComputeCurrencyExchangeSample(markets, currencies)) {
        CurrencyCode cur(curExchange.standardCode());
        log::info("Choosing {} as random currency code for Recent withdraws test", cur);
        WithdrawsSet withdraws = exchangePrivateOpt->queryRecentWithdraws(WithdrawsConstraints(cur));
        if (!withdraws.empty()) {
          EXPECT_EQ(withdraws.front().amount().currencyCode(), cur);
        }
      }
    }
  }

  void testTrade() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    if (!sampleMarkets.empty()) {
      Market mk = sampleMarkets.front();
      LastTradesVector lastTrades = exchangePublic.queryLastTrades(mk);
      if (!lastTrades.empty() && exchangePrivateOpt) {
        auto compareTradedVolume = [](const PublicTrade &lhs, const PublicTrade &rhs) {
          return lhs.amount() < rhs.amount();
        };
        auto [smallAmountIt, bigAmountIt] = std::ranges::minmax_element(lastTrades, compareTradedVolume);

        TradeOptions tradeOptions(TradeMode::kSimulation);
        MonetaryAmount smallFrom = smallAmountIt->amount() / 100;
        MonetaryAmount bigFrom = bigAmountIt->amount().toNeutral() * bigAmountIt->price() * 100;
        EXPECT_GT(exchangePrivateOpt->trade(smallFrom, mk.quote(), tradeOptions).tradedTo, 0);
        EXPECT_NE(exchangePrivateOpt->trade(bigFrom, mk.base(), tradeOptions).tradedFrom, 0);
      }
    }
  }

 private:
  void createPrivateExchangeIfKeyPresent(PublicExchangeT &exchangePublic, const CoincenterInfo &coincenterInfo,
                                         const APIKeysProvider &apiKeysProvider) {
    std::string_view publicExchangeName = exchangePublic.name();
    if (!apiKeysProvider.contains(publicExchangeName)) {
      log::warn("Skip {} private API test as cannot find associated private key", publicExchangeName);
      return;
    }

    ExchangeName exchangeName(publicExchangeName, apiKeysProvider.getKeyNames(publicExchangeName).front());
    const APIKey &firstAPIKey = apiKeysProvider.get(exchangeName);

    exchangePrivateOpt.emplace(coincenterInfo, exchangePublic, firstAPIKey);

    if (!exchangePrivateOpt->validateApiKey()) {
      log::warn("Skip {} private API test as the key has been detected as invalid", exchangeName);
      exchangePrivateOpt.reset();
    }
  }
};

#define CCT_TEST_ALL(TestAPIType, testAPI)                                    \
  TEST(TestAPIType##Test, HealthCheck) { testAPI.testHealthCheck(); }         \
  TEST(TestAPIType##Test, Currencies) { testAPI.testCurrencies(); }           \
  TEST(TestAPIType##Test, Markets) { testAPI.testMarkets(); }                 \
  TEST(TestAPIType##Test, WithdrawalFees) { testAPI.testWithdrawalFees(); }   \
  TEST(TestAPIType##Test, Balance) { testAPI.testBalance(); }                 \
  TEST(TestAPIType##Test, DepositWallet) { testAPI.testDepositWallet(); }     \
  TEST(TestAPIType##Test, RecentDeposits) { testAPI.testRecentDeposits(); }   \
  TEST(TestAPIType##Test, RecentWithdraws) { testAPI.testRecentWithdraws(); } \
  TEST(TestAPIType##Test, Orders) { testAPI.testOpenedOrders(); }             \
  TEST(TestAPIType##Test, Trade) { testAPI.testTrade(); }
}  // namespace cct::api