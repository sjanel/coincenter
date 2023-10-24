#pragma once

#include "cct_json.hpp"

namespace cct {
struct ExchangeInfoDefault {
  static json Prod() {
    // Use a static method instead of an inline static const variable to avoid the infamous 'static initialization order
    // fiasco' problem.

    static const json kProd = R"(
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
      "logLevels": {
        "requestsCall": "info",
        "requestsAnswer": "trace"
      },
      "multiTradeAllowedByDefault": false,
      "placeSimulateRealOrder": false,
      "updateFrequency": {
        "currencies": "8h",
        "markets": "8h",
        "withdrawalFees": "4d",
        "allOrderbooks": "3s",
        "orderbook": "1s",
        "tradedVolume": "1h",
        "lastPrice": "1s",
        "depositWallet": "1min",
        "currencyInfo": "4d"
      },
      "validateApiKey": false
    },
    "exchange": {
      "binance": {
        "acceptEncoding": "gzip",
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
  "tradefees": {
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
)"_json;
    return kProd;
  }

  /// ExchangeInfos for tests only. Some tests rely on provided values so changing them may make them fail.
  static json Test() {
    // Use a static method instead of an inline static const variable to avoid the infamous 'static initialization order
    // fiasco' problem.

    static const json kTest = R"(
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
      "logLevels": {
        "requestsCall": "info",
        "requestsAnswer": "trace"
      },
      "multiTradeAllowedByDefault": true,
      "privateAPIRate": "1055ms",
      "publicAPIRate": "1236ms",
      "placeSimulateRealOrder": false,
      "updateFrequency": {
        "currencies": "8h",
        "markets": "8h",
        "withdrawalFees": "4d",
        "allOrderbooks": "3s",
        "orderbook": "1s",
        "tradedVolume": "1h",
        "lastPrice": "1s",
        "depositWallet": "1min",
        "currencyInfo": "4d"
      },
      "validateApiKey": true
    }
  },
  "tradefees": { 
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
)"_json;
    return kTest;
  }
};
}  // namespace cct