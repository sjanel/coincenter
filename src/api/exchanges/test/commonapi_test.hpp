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
#include "fiatconverter.hpp"

namespace cct {

/// For API tests, standard exchange config is not loaded, we have a dummy one instead.
ExchangeInfoMap ComputeExchangeInfoMap(std::string_view) {
  ExchangeInfoMap ret;
  for (std::string_view exchangeName : kSupportedExchanges) {
    static constexpr bool kValidateDepositAddressesInFile = false;
    static constexpr bool kPlaceSimulatedRealOrder = false;
    static constexpr int kMinPublicDelayMs = 1000;
    static constexpr int kMinPrivateDelayMs = 1000;

    ret.insert_or_assign(
        string(exchangeName),
        ExchangeInfo(exchangeName, "0.1", "0.1", std::span<const CurrencyCode>(), std::span<const CurrencyCode>(),
                     kMinPublicDelayMs, kMinPrivateDelayMs, kValidateDepositAddressesInFile, kPlaceSimulatedRealOrder));
  }
  return ret;
}

namespace api {

template <class PublicExchangeT, class PrivateExchangeT>
class TestAPI {
 public:
  TestAPI()
      : coincenterInfo(settings::RunMode::kProd, kDefaultDataDir),
        coincenterTestInfo(settings::RunMode::kTest),
        apiKeysProvider(coincenterInfo.dataDir(), coincenterInfo.getRunMode()),
        apiTestKeysProvider(coincenterTestInfo.dataDir(), coincenterTestInfo.getRunMode()),
        fiatConverter(coincenterInfo),
        cryptowatchAPI(coincenterInfo),
        exchangePublic(coincenterInfo, fiatConverter, cryptowatchAPI),
        exchangePrivatePtr(CreatePrivateExchangeIfKeyPresent(exchangePublic, coincenterInfo, apiKeysProvider)),
        currencies(exchangePrivatePtr.get() ? exchangePrivatePtr.get()->queryTradableCurrencies()
                                            : exchangePublic.queryTradableCurrencies()),
        markets(exchangePublic.queryTradableMarkets()),
        approximatedMarketOrderbooks(exchangePublic.queryAllApproximatedOrderBooks(1)),
        marketPriceMap(exchangePublic.queryAllPrices()),
        withdrawalFees(exchangePrivatePtr.get() ? exchangePrivatePtr.get()->queryWithdrawalFees()
                                                : exchangePublic.queryWithdrawalFees()) {
    static constexpr int kNbSamples = 1;
    std::sample(markets.begin(), markets.end(), std::inserter(sampleMarkets, sampleMarkets.end()), kNbSamples,
                std::mt19937{std::random_device{}()});
  }

  CoincenterInfo coincenterInfo;
  CoincenterInfo coincenterTestInfo;
  APIKeysProvider apiKeysProvider;
  APIKeysProvider apiTestKeysProvider;
  FiatConverter fiatConverter;
  CryptowatchAPI cryptowatchAPI;
  PublicExchangeT exchangePublic;
  std::unique_ptr<PrivateExchangeT> exchangePrivatePtr;

  CurrencyExchangeFlatSet currencies;
  ExchangePublic::MarketSet markets;
  ExchangePublic::MarketSet sampleMarkets;
  ExchangePublic::MarketOrderBookMap approximatedMarketOrderbooks;
  ExchangePublic::MarketPriceMap marketPriceMap;
  ExchangePublic::WithdrawalFeeMap withdrawalFees;

  void testCurrencies() {
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
    for (Market m : sampleMarkets) {
      testMarket(m);
    }
  }

  void testMarket(Market m) {
    log::info("Test {} market", m.str());
    ASSERT_FALSE(markets.empty());
    static constexpr int kCountDepthOrderBook = 5;
    MarketOrderBook marketOrderBook = exchangePublic.queryOrderBook(m, kCountDepthOrderBook);
    EXPECT_FALSE(marketOrderBook.isArtificiallyExtended());
    if (!marketOrderBook.empty()) {
      EXPECT_LT(marketOrderBook.highestBidPrice(), marketOrderBook.lowestAskPrice());
    }
    EXPECT_NO_THROW(exchangePublic.queryLast24hVolume(m));
    EXPECT_NO_THROW(exchangePublic.queryLastPrice(m));

    auto approximatedOrderbookIt = approximatedMarketOrderbooks.find(m);
    ASSERT_NE(approximatedOrderbookIt, approximatedMarketOrderbooks.end());

    auto marketPriceIt = marketPriceMap.find(m);
    ASSERT_NE(marketPriceIt, marketPriceMap.end());
  }

  void testWithdrawalFees() {
    CurrencyExchangeFlatSet withdrawableCryptos;
    std::copy_if(currencies.begin(), currencies.end(), std::inserter(withdrawableCryptos, withdrawableCryptos.end()),
                 [this](const CurrencyExchange &c) {
                   return !c.isFiat() && c.canWithdraw() && std::any_of(markets.begin(), markets.end(), [&c](Market m) {
                     return m.canTrade(c.standardCode());
                   });
                 });

    if (!withdrawableCryptos.empty()) {
      CurrencyExchangeFlatSet sample;
      if (exchangePublic.isWithdrawalFeesSourceReliable()) {
        std::sample(withdrawableCryptos.begin(), withdrawableCryptos.end(), std::inserter(sample, sample.end()), 1,
                    std::mt19937{std::random_device{}()});
      } else {
        // If exchange withdrawal fees source is not reliable, make several tries
        sample = std::move(withdrawableCryptos);
      }

      for (const CurrencyExchange &curExchange : sample) {
        CurrencyCode cur(curExchange.standardCode());
        log::info("Choosing {} as random currency code for Withdrawal fee test", cur.str());
        auto withdrawalFeeIt = withdrawalFees.find(cur);
        if (exchangePublic.isWithdrawalFeesSourceReliable() || withdrawalFeeIt != withdrawalFees.end()) {
          ASSERT_NE(withdrawalFeeIt, withdrawalFees.end());
          EXPECT_GE(withdrawalFeeIt->second, MonetaryAmount(0, withdrawalFeeIt->second.currencyCode()));
          break;
        } else {
          log::warn("{} withdrawal fee is not known (unreliable source), trying another one", cur.str());
        }
      }
    }
  }

  void testBalance() {
    if (exchangePrivatePtr.get()) {
      EXPECT_NO_THROW(exchangePrivatePtr.get()->getAccountBalance());
    }
  }

  void testDepositWallet() {
    if (exchangePrivatePtr.get()) {
      CurrencyExchangeFlatSet depositableCryptos;
      std::copy_if(currencies.begin(), currencies.end(), std::inserter(depositableCryptos, depositableCryptos.end()),
                   [this](const CurrencyExchange &c) {
                     return !c.isFiat() && c.canDeposit() &&
                            std::any_of(markets.begin(), markets.end(),
                                        [&c](Market m) { return m.canTrade(c.standardCode()); });
                   });
      if (!depositableCryptos.empty()) {
        CurrencyExchangeFlatSet sample;
        if (exchangePrivatePtr.get()->canGenerateDepositAddress()) {
          std::sample(depositableCryptos.begin(), depositableCryptos.end(), std::inserter(sample, sample.end()), 1,
                      std::mt19937{std::random_device{}()});
        } else {
          // If exchange cannot generate deposit wallet, test all until success (hopefully, at least one address will be
          // generated)
          sample = std::move(depositableCryptos);
        }

        for (const CurrencyExchange &curExchange : sample) {
          CurrencyCode cur(curExchange.standardCode());
          log::info("Choosing {} as random currency code for Deposit wallet test", cur.str());
          try {
            Wallet w = exchangePrivatePtr.get()->queryDepositWallet(cur);
            EXPECT_FALSE(w.address().empty());
            break;
          } catch (const exception &) {
            if (exchangePrivatePtr.get()->canGenerateDepositAddress()) {
              throw;
            } else {
              log::info("Wallet for {} is not generated, taking next one", cur.str());
            }
          }
        }
      }
    }
  }

  void testOpenedOrders(Market m) {
    if (exchangePrivatePtr.get()) {
      ExchangePrivate::Orders baseOpenedOrders =
          exchangePrivatePtr.get()->queryOpenedOrders(OrdersConstraints(m.base()));
      if (!baseOpenedOrders.empty()) {
        const Order &openedOrder = baseOpenedOrders.front();
        EXPECT_TRUE(openedOrder.market().canTrade(m.base()));
        EXPECT_LT(openedOrder.matchedVolume(), openedOrder.originalVolume());
      }
    }
  }

  void testTrade(Market m) {
    ExchangePublic::LastTradesVector lastTrades = exchangePublic.queryLastTrades(m);
    if (!lastTrades.empty() && exchangePrivatePtr.get()) {
      auto compareTradedVolume = [](const PublicTrade &lhs, const PublicTrade &rhs) {
        return lhs.amount() < rhs.amount();
      };
      auto [smallAmountIt, bigAmountIt] = std::ranges::minmax_element(lastTrades, compareTradedVolume);

      TradeOptions tradeOptions(TradeMode::kSimulation);
      MonetaryAmount smallFrom = smallAmountIt->amount() / 100;
      MonetaryAmount bigFrom = bigAmountIt->amount().toNeutral() * bigAmountIt->price() * 100;
      EXPECT_GT(exchangePrivatePtr.get()->trade(smallFrom, m.quote(), tradeOptions).tradedTo,
                MonetaryAmount(0, m.quote()));
      EXPECT_FALSE(exchangePrivatePtr.get()->trade(bigFrom, m.base(), tradeOptions).tradedFrom.isZero());
    }
  }

 private:
  static std::unique_ptr<PrivateExchangeT> CreatePrivateExchangeIfKeyPresent(PublicExchangeT &exchangePublic,
                                                                             const CoincenterInfo &coincenterInfo,
                                                                             const APIKeysProvider &apiKeysProvider) {
    std::string_view exchangeName = exchangePublic.name();
    if (!apiKeysProvider.contains(exchangeName)) {
      log::warn("Skip {} private API test as cannot find associated private key", exchangeName);
      return {};
    }

    PrivateExchangeName privateExchangeName(exchangeName, apiKeysProvider.getKeyNames(exchangeName).front());
    const APIKey &firstAPIKey = apiKeysProvider.get(privateExchangeName);
    return std::make_unique<PrivateExchangeT>(coincenterInfo, exchangePublic, firstAPIKey);
  }
};

#define CCT_TEST_ALL(TestAPIType, testAPI)                                                     \
  TEST(TestAPIType##Test, Currencies) { testAPI.testCurrencies(); }                            \
  TEST(TestAPIType##Test, Markets) { testAPI.testMarkets(); }                                  \
  TEST(TestAPIType##Test, WithdrawalFees) { testAPI.testWithdrawalFees(); }                    \
  TEST(TestAPIType##Test, Balance) { testAPI.testBalance(); }                                  \
  TEST(TestAPIType##Test, DepositWallet) { testAPI.testDepositWallet(); }                      \
  TEST(TestAPIType##Test, Orders) { testAPI.testOpenedOrders(testAPI.sampleMarkets.front()); } \
  TEST(TestAPIType##Test, Trade) { testAPI.testTrade(testAPI.sampleMarkets.front()); }
}  // namespace api
}  // namespace cct