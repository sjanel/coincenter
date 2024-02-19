#include <gtest/gtest.h>

#include <string_view>

#include "apioutputtype.hpp"
#include "currencycode.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangepublicapitypes.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "publictrade.hpp"
#include "queryresultprinter.hpp"
#include "queryresultprinter_base_test.hpp"
#include "queryresulttypes.hpp"
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
      .printMarkets(CurrencyCode(), CurrencyCode(), marketsPerExchange);
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
  basicQueryResultPrinter(ApiOutputType::kFormattedTable).printMarkets(cur1, CurrencyCode(), marketsPerExchange);
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
  basicQueryResultPrinter(ApiOutputType::kFormattedTable).printMarkets(cur1, cur2, marketsPerExchange);
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
  basicQueryResultPrinter(ApiOutputType::kJson).printMarkets(cur1, CurrencyCode(), MarketsPerExchange{});
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
  basicQueryResultPrinter(ApiOutputType::kJson).printMarkets(CurrencyCode(), CurrencyCode(), marketsPerExchange);
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
  basicQueryResultPrinter(ApiOutputType::kJson).printMarkets(cur1, CurrencyCode(), marketsPerExchange);
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
  basicQueryResultPrinter(ApiOutputType::kJson).printMarkets(cur1, cur2, marketsPerExchange);
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
  basicQueryResultPrinter(ApiOutputType::kNoPrint).printMarkets(cur1, CurrencyCode(), marketsPerExchange);
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
       TradesVector{PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp1),
                    PublicTrade(TradeSide::kSell, MonetaryAmount{"3.7", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp2),
                    PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.004", "ETH"}, MonetaryAmount{1501, "USDT"}, tp3)}},
      {&exchange3,
       TradesVector{PublicTrade(TradeSide::kSell, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp4),
                    PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.004", "ETH"}, MonetaryAmount{1501, "USDT"}, tp2)}},
      {&exchange2,
       TradesVector{PublicTrade(TradeSide::kSell, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp4),
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

}  // namespace cct