#include <gtest/gtest.h>

#include <array>
#include <string_view>

#include "apioutputtype.hpp"
#include "cct_const.hpp"
#include "coincentercommandtype.hpp"
#include "currencycode.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapitypes.hpp"
#include "market-trading-global-result.hpp"
#include "market-trading-result.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "public-trade-vector.hpp"
#include "publictrade.hpp"
#include "queryresultprinter.hpp"
#include "queryresultprinter_base_test.hpp"
#include "queryresulttypes.hpp"
#include "time-window.hpp"
#include "trade-range-stats.hpp"
#include "tradeside.hpp"

namespace cct {

class QueryResultPrinterHealthCheckTest : public QueryResultPrinterTest {
 protected:
  ExchangeHealthCheckStatus healthCheckPerExchange{{&exchange1, true}, {&exchange4, false}};
};

TEST_F(QueryResultPrinterHealthCheckTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable).printHealthCheck(healthCheckPerExchange);
  static constexpr std::string_view kExpected = R"(
+----------+---------------------+
| Exchange | Health Check status |
+----------+---------------------+
| binance  | OK                  |
| huobi    | Not OK!             |
+----------+---------------------+
)";

  expectStr(kExpected);
}

TEST_F(QueryResultPrinterHealthCheckTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson).printHealthCheck(ExchangeHealthCheckStatus{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "HealthCheck"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterHealthCheckTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson).printHealthCheck(healthCheckPerExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "HealthCheck"
  },
  "out": {
    "binance": true,
    "huobi": false
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterHealthCheckTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint).printHealthCheck(healthCheckPerExchange);
  expectNoStr();
}

class QueryResultPrinterCurrenciesTest : public QueryResultPrinterTest {
 protected:
  CurrencyExchange cur00{"AAVE", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kUnavailable,
                         CurrencyExchange::Type::kCrypto};
  CurrencyExchange cur01{"AAVE", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                         CurrencyExchange::Type::kCrypto};
  CurrencyExchange cur02{"AAVE", CurrencyExchange::Deposit::kUnavailable, CurrencyExchange::Withdraw::kUnavailable,
                         CurrencyExchange::Type::kCrypto};

  CurrencyExchange cur10{"BTC",
                         "XBT",
                         "BTC",
                         CurrencyExchange::Deposit::kAvailable,
                         CurrencyExchange::Withdraw::kAvailable,
                         CurrencyExchange::Type::kCrypto};
  CurrencyExchange cur11{"BTC",
                         "XBTC",
                         CurrencyCode{"BIT"},
                         CurrencyExchange::Deposit::kAvailable,
                         CurrencyExchange::Withdraw::kUnavailable,
                         CurrencyExchange::Type::kCrypto};

  CurrencyExchange cur20{"EUR", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                         CurrencyExchange::Type::kFiat};
  CurrencyExchange cur21{"EUR", CurrencyExchange::Deposit::kUnavailable, CurrencyExchange::Withdraw::kUnavailable,
                         CurrencyExchange::Type::kFiat};

  CurrenciesPerExchange currenciesPerExchange{
      {&exchange1, CurrencyExchangeFlatSet{{cur00, cur10}}},
      {&exchange2, CurrencyExchangeFlatSet{{cur01, cur10, cur21}}},
      {&exchange3, CurrencyExchangeFlatSet{{cur02, cur11, cur20}}},
  };
};

TEST_F(QueryResultPrinterCurrenciesTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable).printCurrencies(currenciesPerExchange);
  static constexpr std::string_view kExpected = R"(
+----------+-----------------------+---------------------------------------+-------------+-----------------------+-------------------+---------+
| Currency | Supported exchanges   | Exchange code(s)                      | Alt code(s) | Can deposit to        | Can withdraw from | Is fiat |
+----------+-----------------------+---------------------------------------+-------------+-----------------------+-------------------+---------+
| AAVE     | binance,bithumb,huobi |                                       |             | binance,bithumb       | bithumb           | no      |
| BTC      | binance,bithumb,huobi | XBT[binance],XBT[bithumb],XBTC[huobi] | BIT[huobi]  | binance,bithumb,huobi | binance,bithumb   | no      |
| EUR      | bithumb,huobi         |                                       |             | huobi                 | huobi             | yes     |
+----------+-----------------------+---------------------------------------+-------------+-----------------------+-------------------+---------+
)";

  expectStr(kExpected);
}

TEST_F(QueryResultPrinterCurrenciesTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson).printCurrencies(CurrenciesPerExchange{});

  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": null,
    "req": "Currencies"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterCurrenciesTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson).printCurrencies(currenciesPerExchange);

  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": null,
    "req": "Currencies"
  },
  "out": {
    "binance": [
      {
        "altCode": "AAVE",
        "canDeposit": true,
        "canWithdraw": false,
        "code": "AAVE",
        "exchangeCode": "AAVE",
        "isFiat": false
      },
      {
        "altCode": "BTC",
        "canDeposit": true,
        "canWithdraw": true,
        "code": "BTC",
        "exchangeCode": "XBT",
        "isFiat": false
      }
    ],
    "bithumb": [
      {
        "altCode": "AAVE",
        "canDeposit": true,
        "canWithdraw": true,
        "code": "AAVE",
        "exchangeCode": "AAVE",
        "isFiat": false
      },
      {
        "altCode": "BTC",
        "canDeposit": true,
        "canWithdraw": true,
        "code": "BTC",
        "exchangeCode": "XBT",
        "isFiat": false
      },
      {
        "altCode": "EUR",
        "canDeposit": false,
        "canWithdraw": false,
        "code": "EUR",
        "exchangeCode": "EUR",
        "isFiat": true
      }
    ],
    "huobi": [
      {
        "altCode": "AAVE",
        "canDeposit": false,
        "canWithdraw": false,
        "code": "AAVE",
        "exchangeCode": "AAVE",
        "isFiat": false
      },
      {
        "altCode": "BIT",
        "canDeposit": true,
        "canWithdraw": false,
        "code": "BTC",
        "exchangeCode": "XBTC",
        "isFiat": false
      },
      {
        "altCode": "EUR",
        "canDeposit": true,
        "canWithdraw": true,
        "code": "EUR",
        "exchangeCode": "EUR",
        "isFiat": true
      }
    ]
  }
})";
  expectJson(kExpected);
}

class QueryResultPrinterMarketsTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode cur1{"XRP"};
  CurrencyCode cur2{"BTC"};
  MarketsPerExchange marketsPerExchange{{&exchange1, MarketSet{Market{cur1, "KRW"}, Market{cur1, cur2}}},
                                        {&exchange2, MarketSet{Market{"SOL", "ETH"}}},
                                        {&exchange3, MarketSet{Market{cur1, "EUR"}}}};
};

TEST_F(QueryResultPrinterMarketsTest, FormattedTableNoCurrency) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable)
      .printMarkets(CurrencyCode(), CurrencyCode(), marketsPerExchange, CoincenterCommandType::kMarkets);
  static constexpr std::string_view kExpected = R"(
+----------+---------+
| Exchange | Markets |
+----------+---------+
| binance  | XRP-BTC |
| binance  | XRP-KRW |
| bithumb  | SOL-ETH |
| huobi    | XRP-EUR |
+----------+---------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterMarketsTest, FormattedTableOneCurrency) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable)
      .printMarkets(cur1, CurrencyCode(), marketsPerExchange, CoincenterCommandType::kMarkets);
  // We only test the title line here, it's normal that all markets are printed (they come from marketsPerExchange and
  // are not filtered again inside the print function)
  static constexpr std::string_view kExpected = R"(
+----------+------------------+
| Exchange | Markets with XRP |
+----------+------------------+
| binance  | XRP-BTC          |
| binance  | XRP-KRW          |
| bithumb  | SOL-ETH          |
| huobi    | XRP-EUR          |
+----------+------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterMarketsTest, FormattedTableTwoCurrencies) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable)
      .printMarkets(cur1, cur2, marketsPerExchange, CoincenterCommandType::kMarkets);
  static constexpr std::string_view kExpected = R"(
+----------+----------------------+
| Exchange | Markets with XRP-BTC |
+----------+----------------------+
| binance  | XRP-BTC              |
| binance  | XRP-KRW              |
| bithumb  | SOL-ETH              |
| huobi    | XRP-EUR              |
+----------+----------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterMarketsTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printMarkets(cur1, CurrencyCode(), MarketsPerExchange{}, CoincenterCommandType::kMarkets);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur1": "XRP"
    },
    "req": "Markets"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterMarketsTest, JsonNoCurrency) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printMarkets(CurrencyCode(), CurrencyCode(), marketsPerExchange, CoincenterCommandType::kMarkets);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
    },
    "req": "Markets"
  },
  "out": {
    "binance": [
      "XRP-BTC",
      "XRP-KRW"
    ],
    "bithumb": [
      "SOL-ETH"
    ],
    "huobi": [
      "XRP-EUR"
    ]
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterMarketsTest, JsonOneCurrency) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printMarkets(cur1, CurrencyCode(), marketsPerExchange, CoincenterCommandType::kMarkets);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur1": "XRP"
    },
    "req": "Markets"
  },
  "out": {
    "binance": [
      "XRP-BTC",
      "XRP-KRW"
    ],
    "bithumb": [
      "SOL-ETH"
    ],
    "huobi": [
      "XRP-EUR"
    ]
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterMarketsTest, JsonTwoCurrencies) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printMarkets(cur1, cur2, marketsPerExchange, CoincenterCommandType::kMarkets);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur1": "XRP",
      "cur2": "BTC"
    },
    "req": "Markets"
  },
  "out": {
    "binance": [
      "XRP-BTC",
      "XRP-KRW"
    ],
    "bithumb": [
      "SOL-ETH"
    ],
    "huobi": [
      "XRP-EUR"
    ]
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterMarketsTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint)
      .printMarkets(cur1, CurrencyCode(), marketsPerExchange, CoincenterCommandType::kMarkets);
  expectNoStr();
}

class QueryResultPrinterTickerTest : public QueryResultPrinterTest {
 protected:
  ExchangeTickerMaps exchangeTickerMaps{
      {&exchange2, MarketOrderBookMap{{Market{"ETH", "EUR"}, this->marketOrderBook11}}},
      {&exchange4, MarketOrderBookMap{{Market{"BTC", "EUR"}, this->marketOrderBook21},
                                      {Market{"XRP", "BTC"}, this->marketOrderBook3}}}};
};

TEST_F(QueryResultPrinterTickerTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable).printTickerInformation(exchangeTickerMaps);
  static constexpr std::string_view kExpected = R"(
+----------+---------+--------------+------------+--------------+------------+
| Exchange | Market  | Bid price    | Bid volume | Ask price    | Ask volume |
+----------+---------+--------------+------------+--------------+------------+
| bithumb  | ETH-EUR | 2301.05 EUR  | 17 ETH     | 2301.15 EUR  | 0.4 ETH    |
| huobi    | BTC-EUR | 31051.01 EUR | 1.9087 BTC | 31051.02 EUR | 0.409 BTC  |
| huobi    | XRP-BTC | 0.36 BTC     | 3494 XRP   | 0.37 BTC     | 916.4 XRP  |
+----------+---------+--------------+------------+--------------+------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterTickerTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson).printTickerInformation(ExchangeTickerMaps{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "Ticker"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterTickerTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson).printTickerInformation(exchangeTickerMaps);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "Ticker"
  },
  "out": {
    "bithumb": [
      {
        "ask": {
          "a": "0.4",
          "p": "2301.15"
        },
        "bid": {
          "a": "17",
          "p": "2301.05"
        },
        "pair": "ETH-EUR"
      }
    ],
    "huobi": [
      {
        "ask": {
          "a": "0.409",
          "p": "31051.02"
        },
        "bid": {
          "a": "1.9087",
          "p": "31051.01"
        },
        "pair": "BTC-EUR"
      },
      {
        "ask": {
          "a": "916.4",
          "p": "0.37"
        },
        "bid": {
          "a": "3494",
          "p": "0.36"
        },
        "pair": "XRP-BTC"
      }
    ]
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterTickerTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint).printTickerInformation(exchangeTickerMaps);
  expectNoStr();
}

class QueryResultPrinterMarketOrderBookTest : public QueryResultPrinterTest {
 protected:
  Market mk{"BTC", "EUR"};
  int d = 3;
  MarketOrderBook mob{tp1,           askPrice2, MonetaryAmount("0.12BTC"), bidPrice2, MonetaryAmount("0.00234 BTC"),
                      volAndPriDec2, d};
  MarketOrderBookConversionRates marketOrderBookConversionRates{{"exchangeA", mob, {}}, {"exchangeD", mob, {}}};
};

TEST_F(QueryResultPrinterMarketOrderBookTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable)
      .printMarketOrderBooks(mk, CurrencyCode{}, d, marketOrderBookConversionRates);
  static constexpr std::string_view kExpected = R"(
+-----------------------+----------------------------+----------------------+
| Sellers of BTC (asks) | exchangeA BTC price in EUR | Buyers of BTC (bids) |
+-----------------------+----------------------------+----------------------+
| 0.18116               | 31056.7                    |                      |
| 0.15058               | 31056.68                   |                      |
| 0.12                  | 31056.67                   |                      |
|                       | 31056.66                   | 0.00234              |
|                       | 31056.65                   | 0.03292              |
|                       | 31056.63                   | 0.0635               |
+-----------------------+----------------------------+----------------------+
+-----------------------+----------------------------+----------------------+
| Sellers of BTC (asks) | exchangeD BTC price in EUR | Buyers of BTC (bids) |
+-----------------------+----------------------------+----------------------+
| 0.18116               | 31056.7                    |                      |
| 0.15058               | 31056.68                   |                      |
| 0.12                  | 31056.67                   |                      |
|                       | 31056.66                   | 0.00234              |
|                       | 31056.65                   | 0.03292              |
|                       | 31056.63                   | 0.0635               |
+-----------------------+----------------------------+----------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterMarketOrderBookTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printMarketOrderBooks(mk, CurrencyCode{}, d, MarketOrderBookConversionRates{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "depth": 3,
      "pair": "BTC-EUR"
    },
    "req": "Orderbook"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterMarketOrderBookTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printMarketOrderBooks(mk, CurrencyCode{}, d, marketOrderBookConversionRates);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "depth": 3,
      "pair": "BTC-EUR"
    },
    "req": "Orderbook"
  },
  "out": {
    "exchangeA": {
      "ask": [
        {
          "a": "0.12",
          "p": "31056.67"
        },
        {
          "a": "0.15058",
          "p": "31056.68"
        },
        {
          "a": "0.18116",
          "p": "31056.7"
        }
      ],
      "bid": [
        {
          "a": "0.00234",
          "p": "31056.66"
        },
        {
          "a": "0.03292",
          "p": "31056.65"
        },
        {
          "a": "0.0635",
          "p": "31056.63"
        }
      ],
      "time": "1999-03-25T04:46:43Z"
    },
    "exchangeD": {
      "ask": [
        {
          "a": "0.12",
          "p": "31056.67"
        },
        {
          "a": "0.15058",
          "p": "31056.68"
        },
        {
          "a": "0.18116",
          "p": "31056.7"
        }
      ],
      "bid": [
        {
          "a": "0.00234",
          "p": "31056.66"
        },
        {
          "a": "0.03292",
          "p": "31056.65"
        },
        {
          "a": "0.0635",
          "p": "31056.63"
        }
      ],
      "time": "1999-03-25T04:46:43Z"
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterMarketOrderBookTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint)
      .printMarketOrderBooks(mk, CurrencyCode{}, d, marketOrderBookConversionRates);
  expectNoStr();
}

class QueryResultPrinterConversionSingleAmountTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount fromAmount{34525, "SOL", 2};
  CurrencyCode targetCurrencyCode{"KRW"};
  MonetaryAmountPerExchange monetaryAmountPerExchange{{&exchange1, MonetaryAmount{41786641, targetCurrencyCode}},
                                                      {&exchange3, MonetaryAmount{44487640, targetCurrencyCode}},
                                                      {&exchange2, MonetaryAmount{59000249, targetCurrencyCode}}};
};

TEST_F(QueryResultPrinterConversionSingleAmountTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable)
      .printConversion(fromAmount, targetCurrencyCode, monetaryAmountPerExchange);
  static constexpr std::string_view kExpected = R"(
+----------+-------------------------------+
| Exchange | 345.25 SOL converted into KRW |
+----------+-------------------------------+
| binance  | 41786641 KRW                  |
| huobi    | 44487640 KRW                  |
| bithumb  | 59000249 KRW                  |
+----------+-------------------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterConversionSingleAmountTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printConversion(fromAmount, targetCurrencyCode, MonetaryAmountPerExchange{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "amount": "345.25 SOL",
      "targetCurrency": "KRW"
    },
    "req": "Conversion"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterConversionSingleAmountTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printConversion(fromAmount, targetCurrencyCode, monetaryAmountPerExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "amount": "345.25 SOL",
      "targetCurrency": "KRW"
    },
    "req": "Conversion"
  },
  "out": {
    "binance": {
      "convertedAmount": "41786641 KRW"
    },
    "bithumb": {
      "convertedAmount": "59000249 KRW"
    },
    "huobi": {
      "convertedAmount": "44487640 KRW"
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterConversionSingleAmountTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint)
      .printConversion(fromAmount, targetCurrencyCode, monetaryAmountPerExchange);
  expectNoStr();
}

class QueryResultPrinterConversionSeveralAmountTest : public QueryResultPrinterTest {
 protected:
  void SetUp() override {
    fromAmounts[0] = MonetaryAmount{1, sourceCurrencyCode, 0};
    fromAmounts[2] = MonetaryAmount{11, sourceCurrencyCode, 1};
    fromAmounts[1] = MonetaryAmount{14, sourceCurrencyCode, 1};
  }

  CurrencyCode sourceCurrencyCode{"BTC"};
  CurrencyCode targetCurrencyCode{"KRW"};
  std::array<MonetaryAmount, kNbSupportedExchanges> fromAmounts;
  MonetaryAmountPerExchange monetaryAmountPerExchange{{&exchange1, MonetaryAmount{41786641, targetCurrencyCode}},
                                                      {&exchange3, MonetaryAmount{44487640, targetCurrencyCode}},
                                                      {&exchange2, MonetaryAmount{59000249, targetCurrencyCode}}};
};

TEST_F(QueryResultPrinterConversionSeveralAmountTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable)
      .printConversion(fromAmounts, targetCurrencyCode, monetaryAmountPerExchange);
  static constexpr std::string_view kExpected = R"(
+----------+---------+--------------+
| Exchange | From    | To           |
+----------+---------+--------------+
| binance  | 1 BTC   | 41786641 KRW |
| huobi    | 1.1 BTC | 44487640 KRW |
| bithumb  | 1.4 BTC | 59000249 KRW |
+----------+---------+--------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterConversionSeveralAmountTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printConversion(fromAmounts, targetCurrencyCode, MonetaryAmountPerExchange{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "sourceAmount": {
        "binance": "1 BTC",
        "bithumb": "1.4 BTC",
        "huobi": "1.1 BTC"
      },
      "targetCurrency": "KRW"
    },
    "req": "Conversion"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterConversionSeveralAmountTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printConversion(fromAmounts, targetCurrencyCode, monetaryAmountPerExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "sourceAmount": {
        "binance": "1 BTC",
        "bithumb": "1.4 BTC",
        "huobi": "1.1 BTC"
      },
      "targetCurrency": "KRW"
    },
    "req": "Conversion"
  },
  "out": {
    "binance": {
      "convertedAmount": "41786641 KRW"
    },
    "bithumb": {
      "convertedAmount": "59000249 KRW"
    },
    "huobi": {
      "convertedAmount": "44487640 KRW"
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterConversionSeveralAmountTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint)
      .printConversion(fromAmounts, targetCurrencyCode, monetaryAmountPerExchange);
  expectNoStr();
}

class QueryResultPrinterConversionPathTest : public QueryResultPrinterTest {
 protected:
  Market marketForPath{"XLM", "XRP"};
  ConversionPathPerExchange conversionPathPerExchange{
      {&exchange1, MarketsPath{}},
      {&exchange2, MarketsPath{Market{"XLM", "XRP"}}},
      {&exchange4, MarketsPath{Market{"XLM", "AAA"}, Market{"BBB", "AAA"}, Market{"BBB", "XRP"}}}};
};

TEST_F(QueryResultPrinterConversionPathTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable).printConversionPath(marketForPath, conversionPathPerExchange);
  static constexpr std::string_view kExpected = R"(
+----------+-------------------------------------+
| Exchange | Fastest conversion path for XLM-XRP |
+----------+-------------------------------------+
| bithumb  | XLM-XRP                             |
| huobi    | XLM-AAA,BBB-AAA,BBB-XRP             |
+----------+-------------------------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterConversionPathTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson).printConversionPath(marketForPath, ConversionPathPerExchange{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "market": "XLM-XRP"
    },
    "req": "ConversionPath"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterConversionPathTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson).printConversionPath(marketForPath, conversionPathPerExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "market": "XLM-XRP"
    },
    "req": "ConversionPath"
  },
  "out": {
    "bithumb": [
      "XLM-XRP"
    ],
    "huobi": [
      "XLM-AAA",
      "BBB-AAA",
      "BBB-XRP"
    ]
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterConversionPathTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint).printConversionPath(marketForPath, conversionPathPerExchange);
  expectNoStr();
}

class QueryResultPrinterWithdrawFeeTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode curWithdrawFee;
  MonetaryAmountByCurrencySetPerExchange withdrawFeesPerExchange{
      {&exchange2, MonetaryAmountByCurrencySet{MonetaryAmount{"0.15", "ETH"}}},
      {&exchange4, MonetaryAmountByCurrencySet{MonetaryAmount{"0.05", "ETH"}, MonetaryAmount{"0.001", "BTC"}}}};
};

TEST_F(QueryResultPrinterWithdrawFeeTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable).printWithdrawFees(withdrawFeesPerExchange, curWithdrawFee);
  static constexpr std::string_view kExpected = R"(
+-----------------------+----------+-----------+
| Withdraw fee currency | bithumb  | huobi     |
+-----------------------+----------+-----------+
| BTC                   |          | 0.001 BTC |
| ETH                   | 0.15 ETH | 0.05 ETH  |
+-----------------------+----------+-----------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterWithdrawFeeTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printWithdrawFees(MonetaryAmountByCurrencySetPerExchange{}, curWithdrawFee);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {},
    "req": "WithdrawFees"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterWithdrawFeeTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson).printWithdrawFees(withdrawFeesPerExchange, curWithdrawFee);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {},
    "req": "WithdrawFees"
  },
  "out": {
    "bithumb": [
      "0.15 ETH"
    ],
    "huobi": [
      "0.001 BTC",
      "0.05 ETH"
    ]
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterWithdrawFeeTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint).printWithdrawFees(withdrawFeesPerExchange, curWithdrawFee);
  expectNoStr();
}

class QueryResultPrinterLast24HoursTradedVolumeTest : public QueryResultPrinterTest {
 protected:
  Market marketLast24hTradedVolume{"BTC", "EUR"};
  MonetaryAmountPerExchange monetaryAmountPerExchange{{&exchange1, MonetaryAmount{"37.8", "BTC"}},
                                                      {&exchange3, MonetaryAmount{"14", "BTC"}}};
};

TEST_F(QueryResultPrinterLast24HoursTradedVolumeTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable)
      .printLast24hTradedVolume(marketLast24hTradedVolume, monetaryAmountPerExchange);
  static constexpr std::string_view kExpected = R"(
+----------+--------------------------------+
| Exchange | Last 24h BTC-EUR traded volume |
+----------+--------------------------------+
| binance  | 37.8 BTC                       |
| huobi    | 14 BTC                         |
+----------+--------------------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterLast24HoursTradedVolumeTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printLast24hTradedVolume(marketLast24hTradedVolume, MonetaryAmountPerExchange{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "market": "BTC-EUR"
    },
    "req": "Last24hTradedVolume"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterLast24HoursTradedVolumeTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printLast24hTradedVolume(marketLast24hTradedVolume, monetaryAmountPerExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "market": "BTC-EUR"
    },
    "req": "Last24hTradedVolume"
  },
  "out": {
    "binance": "37.8",
    "huobi": "14"
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterLast24HoursTradedVolumeTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint)
      .printLast24hTradedVolume(marketLast24hTradedVolume, monetaryAmountPerExchange);
  expectNoStr();
}

class QueryResultPrinterLastTradesVolumeTest : public QueryResultPrinterTest {
 protected:
  Market marketLastTrades{"ETH", "USDT"};
  int nbLastTrades = 3;
  TradesPerExchange lastTradesPerExchange{
      {&exchange1,
       PublicTradeVector{
           PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp1),
           PublicTrade(TradeSide::kSell, MonetaryAmount{"3.7", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp2),
           PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.004", "ETH"}, MonetaryAmount{1501, "USDT"}, tp3)}},
      {&exchange3,
       PublicTradeVector{
           PublicTrade(TradeSide::kSell, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp4),
           PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.004", "ETH"}, MonetaryAmount{1501, "USDT"}, tp2)}},
      {&exchange2,
       PublicTradeVector{
           PublicTrade(TradeSide::kSell, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp4),
           PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.004", "ETH"}, MonetaryAmount{1501, "USDT"}, tp2),
           PublicTrade(TradeSide::kBuy, MonetaryAmount{"47.78", "ETH"}, MonetaryAmount{1498, "USDT"}, tp1)}}};
};

TEST_F(QueryResultPrinterLastTradesVolumeTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable)
      .printLastTrades(marketLastTrades, nbLastTrades, lastTradesPerExchange);
  static constexpr std::string_view kExpected = R"(
+----------------------+--------------------+--------------------------+-------------------+
| binance trades       | ETH buys           | Price in USDT            | ETH sells         |
+----------------------+--------------------+--------------------------+-------------------+
| 1999-03-25T04:46:43Z | 0.13               | 1500.5                   |                   |
| 2002-06-23T07:58:35Z |                    | 1500.5                   | 3.7               |
| 2006-07-14T23:58:24Z | 0.004              | 1501                     |                   |
+----------------------+--------------------+--------------------------+-------------------+
| Summary              | 0.134 ETH (2 buys) | 1500.66666666666666 USDT | 3.7 ETH (1 sells) |
+----------------------+--------------------+--------------------------+-------------------+
+----------------------+--------------------+---------------+--------------------+
| huobi trades         | ETH buys           | Price in USDT | ETH sells          |
+----------------------+--------------------+---------------+--------------------+
| 2011-10-03T06:49:36Z |                    | 1500.5        | 0.13               |
| 2002-06-23T07:58:35Z | 0.004              | 1501          |                    |
+----------------------+--------------------+---------------+--------------------+
| Summary              | 0.004 ETH (1 buys) | 1500.75 USDT  | 0.13 ETH (1 sells) |
+----------------------+--------------------+---------------+--------------------+
+----------------------+---------------------+--------------------------+--------------------+
| bithumb trades       | ETH buys            | Price in USDT            | ETH sells          |
+----------------------+---------------------+--------------------------+--------------------+
| 2011-10-03T06:49:36Z |                     | 1500.5                   | 0.13               |
| 2002-06-23T07:58:35Z | 0.004               | 1501                     |                    |
| 1999-03-25T04:46:43Z | 47.78               | 1498                     |                    |
+----------------------+---------------------+--------------------------+--------------------+
| Summary              | 47.784 ETH (2 buys) | 1499.83333333333333 USDT | 0.13 ETH (1 sells) |
+----------------------+---------------------+--------------------------+--------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterLastTradesVolumeTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson).printLastTrades(marketLastTrades, nbLastTrades, TradesPerExchange{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "market": "ETH-USDT",
      "nb": 3
    },
    "req": "LastTrades"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterLastTradesVolumeTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson).printLastTrades(marketLastTrades, nbLastTrades, lastTradesPerExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "market": "ETH-USDT",
      "nb": 3
    },
    "req": "LastTrades"
  },
  "out": {
    "binance": [
      {
        "a": "0.13",
        "p": "1500.5",
        "side": "Buy",
        "time": "1999-03-25T04:46:43Z"
      },
      {
        "a": "3.7",
        "p": "1500.5",
        "side": "Sell",
        "time": "2002-06-23T07:58:35Z"
      },
      {
        "a": "0.004",
        "p": "1501",
        "side": "Buy",
        "time": "2006-07-14T23:58:24Z"
      }
    ],
    "bithumb": [
      {
        "a": "0.13",
        "p": "1500.5",
        "side": "Sell",
        "time": "2011-10-03T06:49:36Z"
      },
      {
        "a": "0.004",
        "p": "1501",
        "side": "Buy",
        "time": "2002-06-23T07:58:35Z"
      },
      {
        "a": "47.78",
        "p": "1498",
        "side": "Buy",
        "time": "1999-03-25T04:46:43Z"
      }
    ],
    "huobi": [
      {
        "a": "0.13",
        "p": "1500.5",
        "side": "Sell",
        "time": "2011-10-03T06:49:36Z"
      },
      {
        "a": "0.004",
        "p": "1501",
        "side": "Buy",
        "time": "2002-06-23T07:58:35Z"
      }
    ]
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterLastTradesVolumeTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint)
      .printLastTrades(marketLastTrades, nbLastTrades, lastTradesPerExchange);
  expectNoStr();
}

class QueryResultPrinterLastPriceTest : public QueryResultPrinterTest {
 protected:
  Market marketLastPrice{"XRP", "KRW"};
  MonetaryAmountPerExchange monetaryAmountPerExchange{{&exchange1, MonetaryAmount{417, "KRW"}},
                                                      {&exchange3, MonetaryAmount{444, "KRW"}},
                                                      {&exchange2, MonetaryAmount{590, "KRW"}}};
};

TEST_F(QueryResultPrinterLastPriceTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable).printLastPrice(marketLastPrice, monetaryAmountPerExchange);
  static constexpr std::string_view kExpected = R"(
+----------+--------------------+
| Exchange | XRP-KRW last price |
+----------+--------------------+
| binance  | 417 KRW            |
| huobi    | 444 KRW            |
| bithumb  | 590 KRW            |
+----------+--------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterLastPriceTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson).printLastPrice(marketLastPrice, MonetaryAmountPerExchange{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "market": "XRP-KRW"
    },
    "req": "LastPrice"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterLastPriceTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson).printLastPrice(marketLastPrice, monetaryAmountPerExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "market": "XRP-KRW"
    },
    "req": "LastPrice"
  },
  "out": {
    "binance": "417",
    "bithumb": "590",
    "huobi": "444"
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterLastPriceTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint).printLastPrice(marketLastPrice, monetaryAmountPerExchange);
  expectNoStr();
}

class QueryResultPrinterReplayBaseTest : public QueryResultPrinterTest {
 protected:
  Market market1{"ETH", "KRW"};
  Market market2{"BTC", "USD"};
  Market market3{"SHIB", "USDT"};
  Market market4{"SOL", "BTC"};
  Market market5{"SOL", "ETH"};
  Market market6{"ETH", "BTC"};
  Market market7{"DOGE", "CAD"};

  TimePoint tp1{milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{milliseconds{std::numeric_limits<int64_t>::max() / 9900000}};
  TimePoint tp3{milliseconds{std::numeric_limits<int64_t>::max() / 9800000}};
  TimePoint tp4{milliseconds{std::numeric_limits<int64_t>::max() / 9600000}};
  TimePoint tp5{milliseconds{std::numeric_limits<int64_t>::max() / 9500000}};

  TimeWindow timeWindow{tp1, tp5};
};

class QueryResultPrinterReplayMarketsTest : public QueryResultPrinterReplayBaseTest {
 protected:
  MarketTimestampSetsPerExchange marketTimestampSetsPerExchange{
      {&exchange1,
       MarketTimestampSets{MarketTimestampSet{MarketTimestamp{market1, tp1}, MarketTimestamp{market2, tp2},
                                              MarketTimestamp{market3, tp3}},
                           MarketTimestampSet{MarketTimestamp{market1, tp1}, MarketTimestamp{market2, tp1}}}},
      {&exchange2, MarketTimestampSets{MarketTimestampSet{MarketTimestamp{market2, tp4}, MarketTimestamp{market4, tp5}},
                                       MarketTimestampSet{MarketTimestamp{market6, tp1}}}},
      {&exchange3, MarketTimestampSets{MarketTimestampSet{}, MarketTimestampSet{MarketTimestamp{market1, tp1},
                                                                                MarketTimestamp{market7, tp4}}}}};
};

TEST_F(QueryResultPrinterReplayMarketsTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable)
      .printMarketsForReplay(timeWindow, marketTimestampSetsPerExchange);
  static constexpr std::string_view kExpected = R"(
+-----------+--------------------------------+--------------------------------+
| Markets   | Last order books timestamp     | Last trades timestamp          |
+-----------+--------------------------------+--------------------------------+
| BTC-USD   | 1999-07-11T00:42:21Z @ binance | 1999-03-25T04:46:43Z @ binance |
|           | 2000-06-11T23:58:40Z @ bithumb |                                |
|~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
| DOGE-CAD  |                                | 2000-06-11T23:58:40Z @ huobi   |
| ETH-BTC   |                                | 1999-03-25T04:46:43Z @ bithumb |
|~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
| ETH-KRW   | 1999-03-25T04:46:43Z @ binance | 1999-03-25T04:46:43Z @ binance |
|           |                                | 1999-03-25T04:46:43Z @ huobi   |
|~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
| SHIB-USDT | 1999-10-29T01:26:51Z @ binance |                                |
| SOL-BTC   | 2000-10-07T01:14:27Z @ bithumb |                                |
+-----------+--------------------------------+--------------------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterReplayMarketsTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson).printMarketsForReplay(timeWindow, MarketTimestampSetsPerExchange{});
  static constexpr std::string_view kExpected = R"json(
{
  "in": {
    "opt": {
      "timeWindow": "[1999-03-25 04:46:43 -> 2000-10-07 01:14:27)"
    },
    "req": "ReplayMarkets"
  },
  "out": {}
})json";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterReplayMarketsTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson).printMarketsForReplay(timeWindow, marketTimestampSetsPerExchange);
  static constexpr std::string_view kExpected = R"json(
{
  "in": {
    "opt": {
      "timeWindow": "[1999-03-25 04:46:43 -> 2000-10-07 01:14:27)"
    },
    "req": "ReplayMarkets"
  },
  "out": {
    "binance": {
      "orderBooks": [
        {
          "lastTimestamp": "1999-07-11T00:42:21Z",
          "market": "BTC-USD"
        },
        {
          "lastTimestamp": "1999-03-25T04:46:43Z",
          "market": "ETH-KRW"
        },
        {
          "lastTimestamp": "1999-10-29T01:26:51Z",
          "market": "SHIB-USDT"
        }
      ],
      "trades": [
        {
          "lastTimestamp": "1999-03-25T04:46:43Z",
          "market": "BTC-USD"
        },
        {
          "lastTimestamp": "1999-03-25T04:46:43Z",
          "market": "ETH-KRW"
        }
      ]
    },
    "bithumb": {
      "orderBooks": [
        {
          "lastTimestamp": "2000-06-11T23:58:40Z",
          "market": "BTC-USD"
        },
        {
          "lastTimestamp": "2000-10-07T01:14:27Z",
          "market": "SOL-BTC"
        }
      ],
      "trades": [
        {
          "lastTimestamp": "1999-03-25T04:46:43Z",
          "market": "ETH-BTC"
        }
      ]
    },
    "huobi": {
      "orderBooks": null,
      "trades": [
        {
          "lastTimestamp": "2000-06-11T23:58:40Z",
          "market": "DOGE-CAD"
        },
        {
          "lastTimestamp": "1999-03-25T04:46:43Z",
          "market": "ETH-KRW"
        }
      ]
    }
  }
})json";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterReplayMarketsTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint).printMarketsForReplay(timeWindow, marketTimestampSetsPerExchange);
  expectNoStr();
}

class QueryResultPrinterReplayTest : public QueryResultPrinterReplayBaseTest {
 protected:
  ClosedOrder closedOrder1{"1", MonetaryAmount(15, "BTC", 1), MonetaryAmount(35000, "USDT"), tp1, tp1, TradeSide::kBuy};
  ClosedOrder closedOrder2{"2", MonetaryAmount(25, "BTC", 1), MonetaryAmount(45000, "USDT"), tp2, tp2, TradeSide::kBuy};
  ClosedOrder closedOrder3{"3", MonetaryAmount(5, "BTC", 2), MonetaryAmount(35000, "USDT"), tp3, tp4, TradeSide::kSell};
  ClosedOrder closedOrder4{
      "4", MonetaryAmount(17, "BTC", 1), MonetaryAmount(50000, "USDT"), tp3, tp4, TradeSide::kSell};
  ClosedOrder closedOrder5{
      "5", MonetaryAmount(36, "BTC", 3), MonetaryAmount(47899, "USDT"), tp4, tp5, TradeSide::kSell};

  std::string_view algorithmName = "test-algo";
  MonetaryAmount startBaseAmount{1, "BTC"};
  MonetaryAmount startQuoteAmount{1000, "EUR"};

  MarketTradingResult marketTradingResult1{algorithmName, startBaseAmount, startQuoteAmount, MonetaryAmount{0, "EUR"},
                                           ClosedOrderVector{}};
  MarketTradingResult marketTradingResult3{algorithmName, startBaseAmount, startQuoteAmount, MonetaryAmount{500, "EUR"},
                                           ClosedOrderVector{closedOrder1, closedOrder5}};
  MarketTradingResult marketTradingResult4{algorithmName, startBaseAmount, startQuoteAmount, MonetaryAmount{780, "EUR"},
                                           ClosedOrderVector{closedOrder2, closedOrder3, closedOrder4}};

  TradeRangeStats tradeRangeStats1{TradeRangeResultsStats{42, 0}, TradeRangeResultsStats{3, 10}};
  TradeRangeStats tradeRangeStats3{TradeRangeResultsStats{500000, 2}, TradeRangeResultsStats{0, 0}};
  TradeRangeStats tradeRangeStats4{TradeRangeResultsStats{79009, 0}, TradeRangeResultsStats{1555555555, 45}};

  MarketTradingGlobalResultPerExchange marketTradingResultPerExchange{
      {&exchange1, MarketTradingGlobalResult{marketTradingResult1, tradeRangeStats1}},
      {&exchange3, MarketTradingGlobalResult{marketTradingResult3, tradeRangeStats3}},
      {&exchange4, MarketTradingGlobalResult{marketTradingResult4, tradeRangeStats4}}};
  CoincenterCommandType commandType{CoincenterCommandType::kReplay};
};

TEST_F(QueryResultPrinterReplayTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::kFormattedTable)
      .printMarketTradingResults(timeWindow, marketTradingResultPerExchange, commandType);
  static constexpr std::string_view kExpected = R"(
+----------+----------------------+---------+-----------+---------------+---------------+------------------------------------------------------+------------------------------+
| Exchange | Time window          | Market  | Algorithm | Start amounts | Profit / Loss | Matched orders                                       | Stats                        |
+----------+----------------------+---------+-----------+---------------+---------------+------------------------------------------------------+------------------------------+
| binance  | 1999-03-25T04:46:43Z | BTC-EUR | test-algo | 1 BTC         | 0 EUR         |                                                      | order books: 42 OK           |
|          | 2000-10-07T01:14:27Z |         |           | 1000 EUR      |               |                                                      | trades: 3 OK, 10 KO          |
|~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~|~~~~~~~~~~~|~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
| huobi    | 1999-03-25T04:46:43Z | BTC-EUR | test-algo | 1 BTC         | 500 EUR       | 1999-03-25T04:46:43Z - Buy - 1.5 BTC @ 35000 USDT    | order books: 500000 OK, 2 KO |
|          | 2000-10-07T01:14:27Z |         |           | 1000 EUR      |               | 2000-06-11T23:58:40Z - Sell - 0.036 BTC @ 47899 USDT | trades: 0 OK                 |
|~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~|~~~~~~~~~~~|~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
| huobi    | 1999-03-25T04:46:43Z | BTC-EUR | test-algo | 1 BTC         | 780 EUR       | 1999-07-11T00:42:21Z - Buy - 2.5 BTC @ 45000 USDT    | order books: 79009 OK        |
|          | 2000-10-07T01:14:27Z |         |           | 1000 EUR      |               | 1999-10-29T01:26:51Z - Sell - 0.05 BTC @ 35000 USDT  | trades: 1555555555 OK, 45 KO |
|          |                      |         |           |               |               | 1999-10-29T01:26:51Z - Sell - 1.7 BTC @ 50000 USDT   |                              |
+----------+----------------------+---------+-----------+---------------+---------------+------------------------------------------------------+------------------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterReplayTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printMarketTradingResults(timeWindow, MarketTradingGlobalResultPerExchange{}, commandType);
  static constexpr std::string_view kExpected = R"json(
{
  "in": {
    "opt": {
      "time-window": "[1999-03-25 04:46:43 -> 2000-10-07 01:14:27)"
    },
    "req": "Replay"
  },
  "out": {}
})json";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterReplayTest, Json) {
  basicQueryResultPrinter(ApiOutputType::kJson)
      .printMarketTradingResults(timeWindow, marketTradingResultPerExchange, commandType);
  static constexpr std::string_view kExpected = R"json(
{
  "in": {
    "opt": {
      "time-window": "[1999-03-25 04:46:43 -> 2000-10-07 01:14:27)"
    },
    "req": "Replay"
  },
  "out": {
    "binance": {
      "algorithm": "test-algo",
      "market": "BTC-EUR",
      "matched-orders": [],
      "profit-and-loss": "0 EUR",
      "start-amounts": {
        "base": "1 BTC",
        "quote": "1000 EUR"
      },
      "stats": {
        "order-books": {
          "nb-error": 0,
          "nb-successful": 42
        },
        "trades": {
          "nb-error": 10,
          "nb-successful": 3
        }
      }
    },
    "huobi": {
      "algorithm": "test-algo",
      "market": "BTC-EUR",
      "matched-orders": [
        {
          "id": "1",
          "matched": "1.5",
          "matchedTime": "1999-03-25T04:46:43Z",
          "pair": "BTC-USDT",
          "placedTime": "1999-03-25T04:46:43Z",
          "price": "35000",
          "side": "Buy"
        },
        {
          "id": "5",
          "matched": "0.036",
          "matchedTime": "2000-10-07T01:14:27Z",
          "pair": "BTC-USDT",
          "placedTime": "2000-06-11T23:58:40Z",
          "price": "47899",
          "side": "Sell"
        }
      ],
      "profit-and-loss": "500 EUR",
      "start-amounts": {
        "base": "1 BTC",
        "quote": "1000 EUR"
      },
      "stats": {
        "order-books": {
          "nb-error": 2,
          "nb-successful": 500000
        },
        "trades": {
          "nb-error": 0,
          "nb-successful": 0
        }
      }
    }
  }
})json";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterReplayTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::kNoPrint)
      .printMarketTradingResults(timeWindow, marketTradingResultPerExchange, commandType);
  expectNoStr();
}

}  // namespace cct
