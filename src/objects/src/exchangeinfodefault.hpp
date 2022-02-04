#pragma once

#include "cct_json.hpp"

namespace cct {
struct ExchangeInfoDefault {
  static json Prod() {
    // Use a static method instead of an inline static const variable to avoid the infamous 'static initialization order
    // fiasco' problem.

    // clang-format off
    static const json kProd = R"(
{
  "asset": {
    "default": {
      "allExclude": [],
      "withdrawExclude": [
        "BTC",
        "AUD",
        "CAD",
        "GBP",
        "JPY",
        "USD",
        "CHF"
      ]
    },
    "exchange": {
      "binance": {
        "allExclude": [
          "BQX"
        ]
      },
      "bithumb": {
        "withdrawExclude": [
          "KRW"
        ]
      },
      "kraken": {
        "withdrawExclude": [
          "KFEE"
        ]
      },
      "upbit": {
        "withdrawExclude": [
          "KRW"
        ]
      }
    }
  },
  "query": {
    "default": {
      "privateAPIRate": "1000ms",
      "publicAPIRate": "1000ms",
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
        "nbDecimals": "4d"
      }
    },
    "exchange": {
      "binance": {
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
    "default": {
      "maker": "0.1",
      "taker": "0.1"
    },
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
    // clang-format on
    return kProd;
  }

  /// ExchangeInfos for tests only. Some tests rely on provided values so changing them may make them fail.
  static json Test() {
    // Use a static method instead of an inline static const variable to avoid the infamous 'static initialization order
    // fiasco' problem.

    // clang-format off
    static const json kTest = R"(
{
  "asset": {
    "default": {
      "allExclude": [
        "AUD",
        "CAD"
      ],
      "withdrawExclude": [
        "BTC",
        "EUR"
      ]
    }
  },
  "query": {
    "default": {
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
        "nbDecimals": "4d"
      }
    }
  },
  "tradefees": { 
    "default": {
      "maker": "0.16",
      "taker": "0.26"
    }
  },
  "withdraw": {
    "default": {
      "validateDepositAddressesInFile": false
    }
  }
}
)"_json;
    // clang-format on
    return kTest;
  }
};
}  // namespace cct