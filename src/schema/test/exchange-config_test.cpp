#include "exchange-config.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <type_traits>

#include "cct_log.hpp"
#include "cct_string.hpp"
#include "exchange-name-enum.hpp"
#include "priceoptionsdef.hpp"
#include "read-json.hpp"
#include "reader.hpp"

namespace cct::schema {

class ExchangeConfigTest : public ::testing::Test {
 protected:
  class NominalCase : public Reader {
    [[nodiscard]] string readAll() const override {
      return R"(
{
  "general": {
    "default": {
      "enabled": true
    }
  },
  "asset": {
    "default": {
      "allExclude": [],
      "withdrawExclude": [
        "AUD",
        "CAD",
        "CHF",
        "EUR",
        "GBP",
        "JPY",
        "KRW",
        "USD"
      ],
      "preferredPaymentCurrencies": [
        "USDT",
        "USDC"
      ]
    },
    "exchange": {
      "binance": {
        "allExclude": [
          "BQX"
        ]
      },
      "kraken": {
        "withdrawExclude": [
          "KFEE"
        ]
      }
    }
  },
  "query": {
    "default": {
      "acceptEncoding": "",
      "dustAmountsThreshold": [
        "1 EUR",
        "1 USD",
        "1 USDT",
        "1000 KRW",
        "0.000001 BTC"
      ],
      "dustSweeperMaxNbTrades": 7,
      "http": {
        "timeout": "15s"
      },
      "logLevels": {
        "requestsCall": "info",
        "requestsAnswer": "trace"
      },
      "marketDataSerialization": true,
      "multiTradeAllowedByDefault": false,
      "placeSimulateRealOrder": false,
      "trade": {
        "minPriceUpdateDuration": "5s",
        "strategy": "maker",
        "timeout": "30s",
        "timeoutMatch": false
      },
      "updateFrequency": {
        "currencies": "8h",
        "markets": "8h",
        "withdrawalFees": "4d",
        "allOrderBooks": "3s",
        "orderBook": "1s",
        "tradedVolume": "1h",
        "lastPrice": "1s",
        "depositWallet": "1min",
        "currencyInfo": "4d"
      },
      "validateApiKey": false
    },
    "exchange": {
      "binance": {
        "acceptEncoding": "gzip,deflate",
        "privateAPIRate": "150ms",
        "publicAPIRate": "55ms"
      },
      "bithumb": {
        "privateAPIRate": "8ms",
        "publicAPIRate": "8ms"
      },
      "huobi": {
        "privateAPIRate": "100ms",
        "publicAPIRate": "50ms"
      },
      "kraken": {
        "privateAPIRate": "2000ms",
        "publicAPIRate": "500ms"
      },
      "kucoin": {
        "privateAPIRate": "200ms",
        "publicAPIRate": "200ms"
      },
      "upbit": {
        "privateAPIRate": "350ms",
        "publicAPIRate": "100ms"
      }
    }
  },
  "tradeFees": {
    "exchange": {
      "binance": {
        "maker": "0.1",
        "taker": "0.1"
      },
      "bithumb": {
        "maker": "0.25",
        "taker": "0.25"
      },
      "huobi": {
        "maker": "0.2",
        "taker": "0.2"
      },
      "kraken": {
        "maker": "0.16",
        "taker": "0.26"
      },
      "kucoin": {
        "maker": "0.1",
        "taker": "0.1"
      },
      "upbit": {
        "maker": "0.25",
        "taker": "0.25"
      }
    }
  },
  "withdraw": {
    "default": {
      "validateDepositAddressesInFile": true
    }
  }
}
)";
    }
  };
};

TEST_F(ExchangeConfigTest, DirectRead) {
  auto exchangeConfigOptional = ReadJsonOrThrow<details::AllExchangeConfigsOptional>(NominalCase{});

  EXPECT_EQ(exchangeConfigOptional.asset.def.allExclude.size(), 0);
  EXPECT_EQ(exchangeConfigOptional.asset.def.withdrawExclude.size(), 8);
  EXPECT_EQ(exchangeConfigOptional.asset.def.preferredPaymentCurrencies.size(), 2);
  EXPECT_EQ(exchangeConfigOptional.asset.exchange.size(), 2);
  const auto binanceAsset = std::ranges::find_if(
      exchangeConfigOptional.asset.exchange, [](const auto &pair) { return pair.first == ExchangeNameEnum::binance; });
  EXPECT_NE(binanceAsset, exchangeConfigOptional.asset.exchange.end());
  EXPECT_EQ(binanceAsset->second.allExclude.size(), 1);
  const auto krakenAsset = std::ranges::find_if(
      exchangeConfigOptional.asset.exchange, [](const auto &pair) { return pair.first == ExchangeNameEnum::kraken; });
  EXPECT_NE(krakenAsset, exchangeConfigOptional.asset.exchange.end());
  EXPECT_EQ(krakenAsset->second.withdrawExclude.size(), 1);
  EXPECT_EQ(exchangeConfigOptional.query.def.acceptEncoding, "");
  EXPECT_EQ(exchangeConfigOptional.query.def.dustAmountsThreshold.size(), 5);
  EXPECT_EQ(exchangeConfigOptional.query.def.dustSweeperMaxNbTrades, 7);
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  EXPECT_EQ(exchangeConfigOptional.query.def.http->timeout->duration, std::chrono::seconds(15));
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  EXPECT_EQ(exchangeConfigOptional.query.def.logLevels->requestsCall, LogLevel::info);
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  EXPECT_EQ(exchangeConfigOptional.query.def.logLevels->requestsAnswer, LogLevel::trace);
  EXPECT_EQ(exchangeConfigOptional.query.def.marketDataSerialization, true);
  EXPECT_EQ(exchangeConfigOptional.query.def.multiTradeAllowedByDefault, false);
  EXPECT_EQ(exchangeConfigOptional.query.def.placeSimulateRealOrder, false);
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  EXPECT_EQ(exchangeConfigOptional.query.def.trade->minPriceUpdateDuration->duration, std::chrono::seconds(5));
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  EXPECT_EQ(exchangeConfigOptional.query.def.trade->strategy, PriceStrategy::maker);
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  EXPECT_EQ(exchangeConfigOptional.query.def.trade->timeout->duration, std::chrono::seconds(30));
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  EXPECT_EQ(exchangeConfigOptional.query.def.trade->timeoutMatch, false);
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  EXPECT_EQ(exchangeConfigOptional.query.def.validateApiKey, false);
  EXPECT_EQ(exchangeConfigOptional.query.exchange.size(), 6);
  const auto binanceQuery = std::ranges::find_if(
      exchangeConfigOptional.query.exchange, [](const auto &pair) { return pair.first == ExchangeNameEnum::binance; });
  EXPECT_EQ(binanceQuery->second.acceptEncoding, "gzip,deflate");
}

TEST_F(ExchangeConfigTest, ExchangeValuesShouldOverrideDefault) {
  auto exchangeConfigOptional = ReadJsonOrThrow<details::AllExchangeConfigsOptional>(NominalCase{});

  AllExchangeConfigs allExchangeConfigs;

  allExchangeConfigs.mergeWith(exchangeConfigOptional);

  EXPECT_EQ(allExchangeConfigs[ExchangeNameEnum::binance].asset.allExclude, CurrencyCodeSet{"BQX"});
  EXPECT_EQ(allExchangeConfigs[ExchangeNameEnum::kraken].asset.withdrawExclude,
            CurrencyCodeSet({"AUD", "CAD", "CHF", "EUR", "GBP", "JPY", "KRW", "USD", "KFEE"}));
  EXPECT_EQ(allExchangeConfigs[ExchangeNameEnum::binance].query.acceptEncoding, "gzip,deflate");
  EXPECT_EQ(allExchangeConfigs[ExchangeNameEnum::binance].query.privateAPIRate.duration,
            std::chrono::milliseconds(150));
  EXPECT_EQ(allExchangeConfigs[ExchangeNameEnum::binance].query.publicAPIRate.duration, std::chrono::milliseconds(55));
}

}  // namespace cct::schema