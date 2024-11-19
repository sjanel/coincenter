#pragma once

#include <string_view>

namespace cct {

struct ExchangeConfigDefault {
  static constexpr std::string_view kProd = R"(
{
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

  /// ExchangeInfos for tests only. Some tests rely on provided values so changing them may make them fail.
  static constexpr std::string_view kTest = R"(
{
  "asset": {
    "default": {
      "preferredPaymentCurrencies": [
        "USDT",
        "EUR"
      ]
    },
    "exchange": {
      "bithumb": {
        "allExclude": [
          "AUD",
          "CAD"
        ]
      }
    }
  },
  "query": {
    "default": {
      "acceptEncoding": "",
      "dustAmountsThreshold": [
        "1 USDT",
        "1000 KRW",
        "0.00000001 BTC",
        "0.5 XRP"
      ],
      "dustSweeperMaxNbTrades": 5,
      "http": {
        "timeout": "10s"
      },
      "logLevels": {
        "requestsCall": "info",
        "requestsAnswer": "trace"
      },
      "marketDataSerialization": false,
      "multiTradeAllowedByDefault": true,
      "privateAPIRate": "1055ms",
      "publicAPIRate": "1236ms",
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
    }
  },
  "tradeFees": { 
    "default": {
      "maker": "0.1",
      "taker": "0.2"
    }
  },
  "withdraw": {
    "default": {
      "validateDepositAddressesInFile": false
    }
  }
}
)";
};
}  // namespace cct