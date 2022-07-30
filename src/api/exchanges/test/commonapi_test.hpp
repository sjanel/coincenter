#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <random>

#include "apikeysprovider.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"

namespace cct::api {

template <class PublicExchangeT, class PrivateExchangeT>
class TestAPI {
 public:
  static MarketSet ComputeMarketSetSample(const MarketSet &markets, const CurrencyExchangeFlatSet &currencies) {
    static constexpr int kNbSamples = 1;
    MarketSet consideredMarkets;
    std::ranges::copy_if(markets, std::inserter(consideredMarkets, consideredMarkets.end()), [&currencies](Market m) {
      auto cur1It = currencies.find(m.base());
      auto cur2It = currencies.find(m.quote());
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
          return !c.isFiat() && std::ranges::any_of(markets, [&c](Market m) { return m.canTrade(c.standardCode()); });
        });

    CurrencyExchangeFlatSet sample;
    std::ranges::sample(currencyToKeep, std::inserter(sample, sample.end()), 1, std::mt19937{std::random_device{}()});
    return sample;
  }

  LoadConfiguration loadConfig{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  CoincenterInfo coincenterInfo{settings::RunMode::kProd, loadConfig};
  APIKeysProvider apiKeysProvider{coincenterInfo.dataDir(), coincenterInfo.getRunMode()};
  FiatConverter fiatConverter{coincenterInfo, Duration::max()};  // max to avoid real Fiat converter queries
  CryptowatchAPI cryptowatchAPI{coincenterInfo};
  PublicExchangeT exchangePublic{coincenterInfo, fiatConverter, cryptowatchAPI};
  std::unique_ptr<PrivateExchangeT> exchangePrivatePtr{
      CreatePrivateExchangeIfKeyPresent(exchangePublic, coincenterInfo, apiKeysProvider)};

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
    currencies = exchangePrivatePtr.get() ? exchangePrivatePtr.get()->queryTradableCurrencies()
                                          : exchangePublic.queryTradableCurrencies();
    ASSERT_FALSE(currencies.empty());
    EXPECT_TRUE(
        std::ranges::none_of(currencies, [](const CurrencyExchange &c) { return c.standardCode().str().empty(); }));

    // Uncomment below code to print updated Upbit withdrawal fees for static data of withdrawal fees of public API
    // if (exchangePrivatePtr.get()) {
    //   json d;
    //   for (const auto &c : currencies) {
    //     d[string(c.standardStr())] = exchangePrivatePtr.get()->queryWithdrawalFee(c.standardCode()).amountStr();
    //   }
    //   std::cout << d.dump(2) << std::endl;
    // }
  }

  void testMarkets() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    markets = exchangePublic.queryTradableMarkets();
    sampleMarkets = ComputeMarketSetSample(markets, currencies);
    for (Market m : sampleMarkets) {
      testMarket(m);
    }
  }

  void testMarket(Market m) {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    log::info("Test {} market", m);
    ASSERT_FALSE(markets.empty());
    static constexpr int kCountDepthOrderBook = 5;
    MarketOrderBook marketOrderBook = exchangePublic.queryOrderBook(m, kCountDepthOrderBook);
    EXPECT_LE(marketOrderBook.nbAskPrices(), kCountDepthOrderBook);
    EXPECT_LE(marketOrderBook.nbBidPrices(), kCountDepthOrderBook);
    EXPECT_FALSE(marketOrderBook.isArtificiallyExtended());
    if (!marketOrderBook.empty()) {
      EXPECT_LT(marketOrderBook.highestBidPrice(), marketOrderBook.lowestAskPrice());
    }
    EXPECT_NO_THROW(exchangePublic.queryLast24hVolume(m));
    EXPECT_NO_THROW(exchangePublic.queryLastPrice(m));

    MarketOrderBookMap approximatedMarketOrderbooks = exchangePublic.queryAllApproximatedOrderBooks(1);

    auto approximatedOrderbookIt = approximatedMarketOrderbooks.find(m);
    ASSERT_NE(approximatedOrderbookIt, approximatedMarketOrderbooks.end());

    MarketPriceMap marketPriceMap = exchangePublic.queryAllPrices();

    auto marketPriceIt = marketPriceMap.find(m);
    ASSERT_NE(marketPriceIt, marketPriceMap.end());
  }

  void testWithdrawalFees() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    CurrencyExchangeFlatSet withdrawableCryptos;
    std::ranges::copy_if(currencies, std::inserter(withdrawableCryptos, withdrawableCryptos.end()),
                         [this](const CurrencyExchange &c) {
                           return !c.isFiat() && c.canWithdraw() &&
                                  std::ranges::any_of(markets, [&c](Market m) { return m.canTrade(c.standardCode()); });
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

      WithdrawalFeeMap withdrawalFees = exchangePrivatePtr.get() ? exchangePrivatePtr.get()->queryWithdrawalFees()
                                                                 : exchangePublic.queryWithdrawalFees();

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
    if (exchangePrivatePtr.get()) {
      EXPECT_NO_THROW(exchangePrivatePtr.get()->getAccountBalance());
    }
  }

  void testDepositWallet() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    if (exchangePrivatePtr.get()) {
      CurrencyExchangeFlatSet depositableCryptos;
      std::ranges::copy_if(
          currencies, std::inserter(depositableCryptos, depositableCryptos.end()), [this](const CurrencyExchange &c) {
            return !c.isFiat() && c.canDeposit() &&
                   std::ranges::any_of(markets, [&c](Market m) { return m.canTrade(c.standardCode()); });
          });
      if (!depositableCryptos.empty()) {
        CurrencyExchangeFlatSet sample;
        if (exchangePrivatePtr.get()->canGenerateDepositAddress()) {
          std::ranges::sample(depositableCryptos, std::inserter(sample, sample.end()), 1,
                              std::mt19937{std::random_device{}()});
        } else {
          // If exchange cannot generate deposit wallet, test all until success (hopefully, at least one address will be
          // generated)
          sample = std::move(depositableCryptos);
        }

        for (const CurrencyExchange &curExchange : sample) {
          CurrencyCode cur(curExchange.standardCode());
          log::info("Choosing {} as random currency code for Deposit wallet test", cur);
          try {
            Wallet w = exchangePrivatePtr.get()->queryDepositWallet(cur);
            EXPECT_FALSE(w.address().empty());
            break;
          } catch (const exception &) {
            if (exchangePrivatePtr.get()->canGenerateDepositAddress()) {
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
    if (exchangePrivatePtr.get() && !sampleMarkets.empty()) {
      Market m = sampleMarkets.front();
      Orders baseOpenedOrders = exchangePrivatePtr.get()->queryOpenedOrders(OrdersConstraints(m.base()));
      if (!baseOpenedOrders.empty()) {
        const Order &openedOrder = baseOpenedOrders.front();
        EXPECT_TRUE(openedOrder.market().canTrade(m.base()));
        EXPECT_LT(openedOrder.matchedVolume(), openedOrder.originalVolume());
      }
    }
  }

  void testRecentDeposits() {
    if (!exchangeStatusOK) {
      log::warn("Skipping test as exchange has an outage right now");
      return;
    }
    if (exchangePrivatePtr.get()) {
      for (const CurrencyExchange &curExchange : ComputeCurrencyExchangeSample(markets, currencies)) {
        CurrencyCode cur(curExchange.standardCode());
        log::info("Choosing {} as random currency code for Recent deposits test", cur);
        Deposits deposits = exchangePrivatePtr.get()->queryRecentDeposits(DepositsConstraints(cur));
        if (!deposits.empty()) {
          EXPECT_EQ(deposits.front().amount().currencyCode(), cur);
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
      Market m = sampleMarkets.front();
      LastTradesVector lastTrades = exchangePublic.queryLastTrades(m);
      if (!lastTrades.empty() && exchangePrivatePtr.get()) {
        auto compareTradedVolume = [](const PublicTrade &lhs, const PublicTrade &rhs) {
          return lhs.amount() < rhs.amount();
        };
        auto [smallAmountIt, bigAmountIt] = std::ranges::minmax_element(lastTrades, compareTradedVolume);

        TradeOptions tradeOptions(TradeMode::kSimulation);
        MonetaryAmount smallFrom = smallAmountIt->amount() / 100;
        MonetaryAmount bigFrom = bigAmountIt->amount().toNeutral() * bigAmountIt->price() * 100;
        EXPECT_GT(exchangePrivatePtr.get()->trade(smallFrom, m.quote(), tradeOptions).tradedTo, 0);
        EXPECT_NE(exchangePrivatePtr.get()->trade(bigFrom, m.base(), tradeOptions).tradedFrom, 0);
      }
    }
  }

 private:
  static std::unique_ptr<PrivateExchangeT> CreatePrivateExchangeIfKeyPresent(PublicExchangeT &exchangePublic,
                                                                             const CoincenterInfo &coincenterInfo,
                                                                             const APIKeysProvider &apiKeysProvider) {
    std::string_view publicExchangeName = exchangePublic.name();
    if (!apiKeysProvider.contains(publicExchangeName)) {
      log::warn("Skip {} private API test as cannot find associated private key", publicExchangeName);
      return {};
    }

    ExchangeName exchangeName(publicExchangeName, apiKeysProvider.getKeyNames(publicExchangeName).front());
    const APIKey &firstAPIKey = apiKeysProvider.get(exchangeName);
    return std::make_unique<PrivateExchangeT>(coincenterInfo, exchangePublic, firstAPIKey);
  }
};

#define CCT_TEST_ALL(TestAPIType, testAPI)                                  \
  TEST(TestAPIType##Test, HealthCheck) { testAPI.testHealthCheck(); }       \
  TEST(TestAPIType##Test, Currencies) { testAPI.testCurrencies(); }         \
  TEST(TestAPIType##Test, Markets) { testAPI.testMarkets(); }               \
  TEST(TestAPIType##Test, WithdrawalFees) { testAPI.testWithdrawalFees(); } \
  TEST(TestAPIType##Test, Balance) { testAPI.testBalance(); }               \
  TEST(TestAPIType##Test, DepositWallet) { testAPI.testDepositWallet(); }   \
  TEST(TestAPIType##Test, RecentDeposits) { testAPI.testRecentDeposits(); } \
  TEST(TestAPIType##Test, Orders) { testAPI.testOpenedOrders(); }           \
  TEST(TestAPIType##Test, Trade) { testAPI.testTrade(); }
}  // namespace cct::api