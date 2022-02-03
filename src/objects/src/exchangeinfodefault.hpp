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
      "allexclude": "",
      "withdrawexclude": "BTC,AUD,CAD,GBP,JPY,USD,CHF"
    },
    "exchange": {
      "binance": {
        "allexclude": "BQX"
      },
      "bithumb": {
        "withdrawexclude": "KRW"
      },
      "kraken": {
        "withdrawexclude": "KFEE"
      },
      "upbit": {
        "withdrawexclude": "KRW"
      }
    }
  },
  "query": {
    "default": {
      "minprivatequerydelayms": 1000,
      "minpublicquerydelayms": 1000,
      "placesimulaterealorder": false,
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
        "minprivatequerydelayms": 150,
        "minpublicquerydelayms": 55
      },
      "bithumb": {
        "minprivatequerydelayms": 8,
        "minpublicquerydelayms": 8
      },
      "huobi": {
        "minprivatequerydelayms": 100,
        "minpublicquerydelayms": 50
      },
      "kraken": {
        "minprivatequerydelayms": 2000,
        "minpublicquerydelayms": 500
      },
      "kucoin": {
        "minprivatequerydelayms": 200,
        "minpublicquerydelayms": 200
      },
      "upbit": {
        "minprivatequerydelayms": 350,
        "minpublicquerydelayms": 100
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
      "validatedepositaddressesinfile": true
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
      "allexclude": "AUD,CAD",
      "withdrawexclude": "BTC,EUR"
    }
  },
  "query": {
    "default": {
      "minprivatequerydelayms": 1055,
      "minpublicquerydelayms": 1236,
      "placesimulaterealorder": false,
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
      "validatedepositaddressesinfile": false
    }
  }
}
)"_json;
    // clang-format on
    return kTest;
  }
};
}  // namespace cct