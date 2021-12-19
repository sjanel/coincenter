#pragma once

#include "cct_json.hpp"

namespace cct {
// clang-format off
const json kDefaultConfig = R"(
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
      "placesimulaterealorder": false
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
}  // namespace cct