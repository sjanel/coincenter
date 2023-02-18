#include "queryresultprinter.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string_view>

#include "cct_config.hpp"
#include "exchangedata_test.hpp"

namespace cct {

class QueryResultPrinterTest : public ExchangesBaseTest {
 protected:
  TimePoint tp1{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 9000000}};
  TimePoint tp3{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 8000000}};
  TimePoint tp4{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 7000000}};

  void SetUp() override { ss.clear(); }

  void expectNoStr() const { EXPECT_TRUE(ss.view().empty()); }

  void expectStr(std::string_view expected) const {
    ASSERT_FALSE(expected.empty());
    expected.remove_prefix(1);  // skip first newline char of expected string
    EXPECT_EQ(ss.view(), expected);
  }

  void expectJson(std::string_view expected) const {
    ASSERT_FALSE(expected.empty());
    expected.remove_prefix(1);  // skip first newline char of expected string
    EXPECT_EQ(json::parse(ss.view()), json::parse(expected));
  }

  std::ostringstream ss;
};

class QueryResultPrinterHealthCheckTest : public QueryResultPrinterTest {
 protected:
  ExchangeHealthCheckStatus healthCheckPerExchange{{&exchange1, true}, {&exchange4, false}};
};

TEST_F(QueryResultPrinterHealthCheckTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printHealthCheck(healthCheckPerExchange);
  static constexpr std::string_view kExpected = R"(
----------------------------------
| Exchange | Health Check status |
----------------------------------
| binance  | OK                  |
| huobi    | Not OK!             |
----------------------------------
)";

  expectStr(kExpected);
}

TEST_F(QueryResultPrinterHealthCheckTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printHealthCheck(ExchangeHealthCheckStatus{});
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
  QueryResultPrinter(ss, ApiOutputType::kJson).printHealthCheck(healthCheckPerExchange);
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
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printHealthCheck(healthCheckPerExchange);
  expectNoStr();
}

class QueryResultPrinterMarketsTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode cur1{"XRP"};
  CurrencyCode cur2;
  MarketsPerExchange marketsPerExchange{{&exchange1, MarketSet{Market{cur1, "KRW"}, Market{cur1, "BTC"}}},
                                        {&exchange3, MarketSet{Market{cur1, "EUR"}}}};
};

TEST_F(QueryResultPrinterMarketsTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printMarkets(cur1, cur2, marketsPerExchange);
  static constexpr std::string_view kExpected = R"(
-------------------------------
| Exchange | Markets with XRP |
-------------------------------
| binance  | XRP-BTC          |
| binance  | XRP-KRW          |
| huobi    | XRP-EUR          |
-------------------------------
)";

  expectStr(kExpected);
}

TEST_F(QueryResultPrinterMarketsTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printMarkets(cur1, cur2, MarketsPerExchange{});
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

TEST_F(QueryResultPrinterMarketsTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printMarkets(cur1, cur2, marketsPerExchange);
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
    "huobi": [
      "XRP-EUR"
    ]
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterMarketsTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printMarkets(cur1, cur2, marketsPerExchange);
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
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printTickerInformation(exchangeTickerMaps);
  static constexpr std::string_view kExpected = R"(
------------------------------------------------------------------------------
| Exchange | Market  | Bid price    | Bid volume | Ask price    | Ask volume |
------------------------------------------------------------------------------
| bithumb  | ETH-EUR | 2301.05 EUR  | 17 ETH     | 2301.15 EUR  | 0.4 ETH    |
| huobi    | BTC-EUR | 31051.01 EUR | 1.9087 BTC | 31051.02 EUR | 0.409 BTC  |
| huobi    | XRP-BTC | 0.36 BTC     | 3494 XRP   | 0.37 BTC     | 916.4 XRP  |
------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterTickerTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printTickerInformation(ExchangeTickerMaps{});
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
  QueryResultPrinter(ss, ApiOutputType::kJson).printTickerInformation(exchangeTickerMaps);
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
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printTickerInformation(exchangeTickerMaps);
  expectNoStr();
}

class QueryResultPrinterMarketOrderBookTest : public QueryResultPrinterTest {
 protected:
  Market m{"BTC", "EUR"};
  int d = 3;
  MarketOrderBook mob{askPrice2, MonetaryAmount("0.12BTC"), bidPrice2, MonetaryAmount("0.00234 BTC"), volAndPriDec2, d};
  MarketOrderBookConversionRates marketOrderBookConversionRates{{"exchangeA", mob, {}}, {"exchangeD", mob, {}}};
};

TEST_F(QueryResultPrinterMarketOrderBookTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printMarketOrderBooks(m, CurrencyCode{}, d, marketOrderBookConversionRates);
  static constexpr std::string_view kExpected = R"(
-----------------------------------------------------------------------------
| Sellers of BTC (asks) | exchangeA BTC price in EUR | Buyers of BTC (bids) |
-----------------------------------------------------------------------------
| 0.18116               | 31056.7                    |                      |
| 0.15058               | 31056.68                   |                      |
| 0.12                  | 31056.67                   |                      |
|                       | 31056.66                   | 0.00234              |
|                       | 31056.65                   | 0.03292              |
|                       | 31056.63                   | 0.0635               |
-----------------------------------------------------------------------------
-----------------------------------------------------------------------------
| Sellers of BTC (asks) | exchangeD BTC price in EUR | Buyers of BTC (bids) |
-----------------------------------------------------------------------------
| 0.18116               | 31056.7                    |                      |
| 0.15058               | 31056.68                   |                      |
| 0.12                  | 31056.67                   |                      |
|                       | 31056.66                   | 0.00234              |
|                       | 31056.65                   | 0.03292              |
|                       | 31056.63                   | 0.0635               |
-----------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterMarketOrderBookTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson)
      .printMarketOrderBooks(m, CurrencyCode{}, d, MarketOrderBookConversionRates{});
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
  QueryResultPrinter(ss, ApiOutputType::kJson)
      .printMarketOrderBooks(m, CurrencyCode{}, d, marketOrderBookConversionRates);
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
      ]
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
      ]
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterMarketOrderBookTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint)
      .printMarketOrderBooks(m, CurrencyCode{}, d, marketOrderBookConversionRates);
  expectNoStr();
}

class QueryResultPrinterEmptyBalanceNoEquiCurTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode equiCur;
  BalancePortfolio emptyBal;
  BalancePerExchange balancePerExchange{{&exchange1, emptyBal}, {&exchange4, emptyBal}};
};

TEST_F(QueryResultPrinterEmptyBalanceNoEquiCurTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printBalance(balancePerExchange, equiCur);
  static constexpr std::string_view kExpected = R"(
-----------------------------------------------------------------------------
| Currency | Total amount on selected | binance_testuser1 | huobi_testuser2 |
-----------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterEmptyBalanceNoEquiCurTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printBalance(BalancePerExchange{}, equiCur);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {},
    "req": "Balance"
  },
  "out": {
    "exchange": {},
    "total": {
      "cur": {}
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterEmptyBalanceNoEquiCurTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printBalance(balancePerExchange, equiCur);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {},
    "req": "Balance"
  },
  "out": {
    "exchange": {
      "binance": {
        "testuser1": {}
      },
      "huobi": {
        "testuser2": {}
      }
    },
    "total": {
      "cur": {}
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterEmptyBalanceNoEquiCurTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printBalance(balancePerExchange, equiCur);
  expectNoStr();
}

class QueryResultPrinterBalanceNoEquiCurTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode equiCur;
  BalancePortfolio bp3;
  BalancePerExchange balancePerExchange{
      {&exchange1, balancePortfolio1}, {&exchange4, balancePortfolio4}, {&exchange2, bp3}};
};

TEST_F(QueryResultPrinterBalanceNoEquiCurTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printBalance(balancePerExchange, equiCur);
  static constexpr std::string_view kExpected = R"(
-------------------------------------------------------------------------------------------------
| Currency | Total amount on selected | binance_testuser1 | huobi_testuser2 | bithumb_testuser1 |
-------------------------------------------------------------------------------------------------
| ADA      | 147                      | 0                 | 147             | 0                 |
| BTC      | 15                       | 15                | 0               | 0                 |
| DOT      | 4.76                     | 0                 | 4.76            | 0                 |
| ETH      | 1.5                      | 1.5               | 0               | 0                 |
| EUR      | 1200                     | 0                 | 1200            | 0                 |
| MATIC    | 15004                    | 0                 | 15004           | 0                 |
| USD      | 155                      | 0                 | 155             | 0                 |
| USDT     | 5107.5                   | 5000              | 107.5           | 0                 |
| XRP      | 1500                     | 1500              | 0               | 0                 |
-------------------------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterBalanceNoEquiCurTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printBalance(BalancePerExchange{}, equiCur);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {},
    "req": "Balance"
  },
  "out": {
    "exchange": {},
    "total": {
      "cur": {}
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterBalanceNoEquiCurTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printBalance(balancePerExchange, equiCur);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {},
    "req": "Balance"
  },
  "out": {
    "exchange": {
      "binance": {
        "testuser1": {
          "BTC": {
            "a": "15"
          },
          "ETH": {
            "a": "1.5"
          },
          "USDT": {
            "a": "5000"
          },
          "XRP": {
            "a": "1500"
          }
        }
      },
      "bithumb": {
        "testuser1": {}
      },
      "huobi": {
        "testuser2": {
          "ADA": {
            "a": "147"
          },
          "DOT": {
            "a": "4.76"
          },
          "EUR": {
            "a": "1200"
          },
          "MATIC": {
            "a": "15004"
          },
          "USD": {
            "a": "155"
          },
          "USDT": {
            "a": "107.5"
          }
        }
      }
    },
    "total": {
      "cur": {
        "ADA": {
          "a": "147"
        },
        "BTC": {
          "a": "15"
        },
        "DOT": {
          "a": "4.76"
        },
        "ETH": {
          "a": "1.5"
        },
        "EUR": {
          "a": "1200"
        },
        "MATIC": {
          "a": "15004"
        },
        "USD": {
          "a": "155"
        },
        "USDT": {
          "a": "5107.5"
        },
        "XRP": {
          "a": "1500"
        }
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterBalanceNoEquiCurTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printBalance(balancePerExchange, equiCur);
  expectNoStr();
}

class QueryResultPrinterBalanceEquiCurTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode equiCur{"EUR"};
  BalancePortfolio bp1{{MonetaryAmount("15000ADA"), MonetaryAmount("10000EUR")},
                       {MonetaryAmount("0.56BTC"), MonetaryAmount("9067.7EUR")}};
  BalancePortfolio bp2{{MonetaryAmount("34.7XRP"), MonetaryAmount("45.08EUR")},
                       {MonetaryAmount("15ETH"), MonetaryAmount("25000EUR")},
                       {MonetaryAmount("123XLM"), MonetaryAmount("67.5EUR")}};
  BalancePortfolio bp3;
  BalancePerExchange balancePerExchange{{&exchange1, bp1}, {&exchange4, bp2}, {&exchange2, bp3}, {&exchange3, bp3}};
};

TEST_F(QueryResultPrinterBalanceEquiCurTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printBalance(balancePerExchange, equiCur);
  static constexpr std::string_view kExpected = R"(
----------------------------------------------------------------------------------------------------------------------------------
| Currency | Total amount on selected | Total EUR eq | binance_testuser1 | huobi_testuser2 | bithumb_testuser1 | huobi_testuser1 |
----------------------------------------------------------------------------------------------------------------------------------
| ETH      | 15                       | 25000        | 0                 | 15              | 0                 | 0               |
| ADA      | 15000                    | 10000        | 15000             | 0               | 0                 | 0               |
| BTC      | 0.56                     | 9067.7       | 0.56              | 0               | 0                 | 0               |
| XLM      | 123                      | 67.5         | 0                 | 123             | 0                 | 0               |
| XRP      | 34.7                     | 45.08        | 0                 | 34.7            | 0                 | 0               |
----------------------------------------------------------------------------------------------------------------------------------
| Total    |                          | 44180.28     |                   |                 |                   |                 |
----------------------------------------------------------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterBalanceEquiCurTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printBalance(BalancePerExchange{}, equiCur);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "equiCurrency": "EUR"
    },
    "req": "Balance"
  },
  "out": {
    "exchange": {},
    "total": {
      "cur": {},
      "eq": "0"
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterBalanceEquiCurTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printBalance(balancePerExchange, equiCur);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "equiCurrency": "EUR"
    },
    "req": "Balance"
  },
  "out": {
    "exchange": {
      "binance": {
        "testuser1": {
          "ADA": {
            "a": "15000",
            "eq": "10000"
          },
          "BTC": {
            "a": "0.56",
            "eq": "9067.7"
          }
        }
      },
      "bithumb": {
        "testuser1": {}
      },
      "huobi": {
        "testuser1": {},
        "testuser2": {
          "ETH": {
            "a": "15",
            "eq": "25000"
          },
          "XLM": {
            "a": "123",
            "eq": "67.5"
          },
          "XRP": {
            "a": "34.7",
            "eq": "45.08"
          }
        }
      }
    },
    "total": {
      "cur": {
        "ADA": {
          "a": "15000",
          "eq": "10000"
        },
        "BTC": {
          "a": "0.56",
          "eq": "9067.7"
        },
        "ETH": {
          "a": "15",
          "eq": "25000"
        },
        "XLM": {
          "a": "123",
          "eq": "67.5"
        },
        "XRP": {
          "a": "34.7",
          "eq": "45.08"
        }
      },
      "eq": "44180.28"
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterBalanceEquiCurTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printBalance(balancePerExchange, equiCur);
  expectNoStr();
}

class QueryResultPrinterDepositInfoWithoutTagTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode depositCurrencyCode{"ETH"};
  WalletPerExchange walletPerExchange{{&exchange2, Wallet{exchange2.apiPrivate().exchangeName(), depositCurrencyCode,
                                                          "ethaddress666", "", WalletCheck{}}},
                                      {&exchange4, Wallet{exchange4.apiPrivate().exchangeName(), depositCurrencyCode,
                                                          "ethaddress667", "", WalletCheck{}}}};
};

TEST_F(QueryResultPrinterDepositInfoWithoutTagTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printDepositInfo(depositCurrencyCode, walletPerExchange);
  static constexpr std::string_view kExpected = R"(
----------------------------------------------------------
| Exchange | Account   | ETH address   | Destination Tag |
----------------------------------------------------------
| bithumb  | testuser1 | ethaddress666 |                 |
| huobi    | testuser2 | ethaddress667 |                 |
----------------------------------------------------------
)";

  expectStr(kExpected);
}

TEST_F(QueryResultPrinterDepositInfoWithoutTagTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printDepositInfo(depositCurrencyCode, WalletPerExchange{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "ETH"
    },
    "req": "DepositInfo"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterDepositInfoWithoutTagTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printDepositInfo(depositCurrencyCode, walletPerExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "ETH"
    },
    "req": "DepositInfo"
  },
  "out": {
    "bithumb": {
      "testuser1": {
        "address": "ethaddress666"
      }
    },
    "huobi": {
      "testuser2": {
        "address": "ethaddress667"
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterDepositInfoWithoutTagTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printDepositInfo(depositCurrencyCode, walletPerExchange);
  expectNoStr();
}

class QueryResultPrinterDepositInfoWithTagTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode depositCurrencyCode{"XRP"};
  WalletPerExchange walletPerExchange{{&exchange3, Wallet{exchange3.apiPrivate().exchangeName(), depositCurrencyCode,
                                                          "xrpaddress666", "xrptag1", WalletCheck{}}},
                                      {&exchange4, Wallet{exchange4.apiPrivate().exchangeName(), depositCurrencyCode,
                                                          "xrpaddress666", "xrptag2", WalletCheck{}}}};
};

TEST_F(QueryResultPrinterDepositInfoWithTagTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printDepositInfo(depositCurrencyCode, walletPerExchange);
  static constexpr std::string_view kExpected = R"(
----------------------------------------------------------
| Exchange | Account   | XRP address   | Destination Tag |
----------------------------------------------------------
| huobi    | testuser1 | xrpaddress666 | xrptag1         |
| huobi    | testuser2 | xrpaddress666 | xrptag2         |
----------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterDepositInfoWithTagTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printDepositInfo(depositCurrencyCode, WalletPerExchange{});
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "XRP"
    },
    "req": "DepositInfo"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterDepositInfoWithTagTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printDepositInfo(depositCurrencyCode, walletPerExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "XRP"
    },
    "req": "DepositInfo"
  },
  "out": {
    "huobi": {
      "testuser1": {
        "address": "xrpaddress666",
        "tag": "xrptag1"
      },
      "testuser2": {
        "address": "xrpaddress666",
        "tag": "xrptag2"
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterDepositInfoWithTagTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printDepositInfo(depositCurrencyCode, walletPerExchange);
  expectNoStr();
}

class QueryResultPrinterTradesAmountTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount startAmount{"0.5BTC"};
  bool isPercentageTrade{false};
  CurrencyCode toCurrency{"XRP"};
  TradeOptions tradeOptions;
  TradedAmountsPerExchange tradedAmountsPerExchange{
      {&exchange1, TradedAmounts{MonetaryAmount("0.1BTC"), MonetaryAmount("1050XRP")}},
      {&exchange3, TradedAmounts{MonetaryAmount("0.3BTC"), MonetaryAmount("3500.6XRP")}},
      {&exchange4, TradedAmounts{MonetaryAmount(0, "BTC"), MonetaryAmount(0, "XRP")}}};
};

TEST_F(QueryResultPrinterTradesAmountTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, toCurrency, tradeOptions);
  static constexpr std::string_view kExpected = R"(
------------------------------------------------------------------------------
| Exchange | Account   | Traded from amount (real) | Traded to amount (real) |
------------------------------------------------------------------------------
| binance  | testuser1 | 0.1 BTC                   | 1050 XRP                |
| huobi    | testuser1 | 0.3 BTC                   | 3500.6 XRP              |
| huobi    | testuser2 | 0 BTC                     | 0 XRP                   |
------------------------------------------------------------------------------
)";

  expectStr(kExpected);
}

TEST_F(QueryResultPrinterTradesAmountTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson)
      .printTrades(TradedAmountsPerExchange{}, startAmount, isPercentageTrade, toCurrency, tradeOptions);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "from": {
        "amount": "0.5",
        "currency": "BTC",
        "isPercentage": false
      },
      "options": {
        "maxTradeTime": "30s",
        "minTimeBetweenPriceUpdates": "5s",
        "mode": "real",
        "price": {
          "strategy": "maker"
        },
        "timeoutAction": "cancel"
      },
      "to": {
        "currency": "XRP"
      }
    },
    "req": "Trade"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterTradesAmountTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson)
      .printTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, toCurrency, tradeOptions);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "from": {
        "amount": "0.5",
        "currency": "BTC",
        "isPercentage": false
      },
      "options": {
        "maxTradeTime": "30s",
        "minTimeBetweenPriceUpdates": "5s",
        "mode": "real",
        "price": {
          "strategy": "maker"
        },
        "timeoutAction": "cancel"
      },
      "to": {
        "currency": "XRP"
      }
    },
    "req": "Trade"
  },
  "out": {
    "binance": {
      "testuser1": {
        "from": "0.1",
        "to": "1050"
      }
    },
    "huobi": {
      "testuser1": {
        "from": "0.3",
        "to": "3500.6"
      },
      "testuser2": {
        "from": "0",
        "to": "0"
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterTradesAmountTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint)
      .printTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, toCurrency, tradeOptions);
  expectNoStr();
}

class QueryResultPrinterTradesPercentageTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount startAmount{"25.6EUR"};
  bool isPercentageTrade{true};
  CurrencyCode toCurrency{"SHIB"};
  TradeOptions tradeOptions{PriceOptions{PriceStrategy::kTaker}};
  TradedAmountsPerExchange tradedAmountsPerExchange{
      {&exchange2, TradedAmounts{MonetaryAmount("15000.56EUR"), MonetaryAmount("885475102SHIB")}}};
};

TEST_F(QueryResultPrinterTradesPercentageTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, toCurrency, tradeOptions);
  static constexpr std::string_view kExpected = R"(
------------------------------------------------------------------------------
| Exchange | Account   | Traded from amount (real) | Traded to amount (real) |
------------------------------------------------------------------------------
| bithumb  | testuser1 | 15000.56 EUR              | 885475102 SHIB          |
------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterTradesPercentageTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson)
      .printTrades(TradedAmountsPerExchange{}, startAmount, isPercentageTrade, toCurrency, tradeOptions);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "from": {
        "amount": "25.6",
        "currency": "EUR",
        "isPercentage": true
      },
      "options": {
        "maxTradeTime": "30s",
        "minTimeBetweenPriceUpdates": "5s",
        "mode": "real",
        "price": {
          "strategy": "taker"
        },
        "timeoutAction": "cancel"
      },
      "to": {
        "currency": "SHIB"
      }
    },
    "req": "Trade"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterTradesPercentageTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson)
      .printTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, toCurrency, tradeOptions);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "from": {
        "amount": "25.6",
        "currency": "EUR",
        "isPercentage": true
      },
      "options": {
        "maxTradeTime": "30s",
        "minTimeBetweenPriceUpdates": "5s",
        "mode": "real",
        "price": {
          "strategy": "taker"
        },
        "timeoutAction": "cancel"
      },
      "to": {
        "currency": "SHIB"
      }
    },
    "req": "Trade"
  },
  "out": {
    "bithumb": {
      "testuser1": {
        "from": "15000.56",
        "to": "885475102"
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterTradesPercentageTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint)
      .printTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, toCurrency, tradeOptions);
  expectNoStr();
}

class QueryResultPrinterSmartBuyTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount endAmount{"3ETH"};
  TradeOptions tradeOptions;
  TradedAmountsPerExchange tradedAmountsPerExchange{
      {&exchange1, TradedAmounts{MonetaryAmount("4500.67EUR"), MonetaryAmount("3ETH")}}};
};

TEST_F(QueryResultPrinterSmartBuyTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printBuyTrades(tradedAmountsPerExchange, endAmount, tradeOptions);
  static constexpr std::string_view kExpected = R"(
------------------------------------------------------------------------------
| Exchange | Account   | Traded from amount (real) | Traded to amount (real) |
------------------------------------------------------------------------------
| binance  | testuser1 | 4500.67 EUR               | 3 ETH                   |
------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterSmartBuyTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printBuyTrades(TradedAmountsPerExchange{}, endAmount, tradeOptions);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "options": {
        "maxTradeTime": "30s",
        "minTimeBetweenPriceUpdates": "5s",
        "mode": "real",
        "price": {
          "strategy": "maker"
        },
        "timeoutAction": "cancel"
      },
      "to": {
        "amount": "3",
        "currency": "ETH",
        "isPercentage": false
      }
    },
    "req": "Buy"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterSmartBuyTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printBuyTrades(tradedAmountsPerExchange, endAmount, tradeOptions);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "options": {
        "maxTradeTime": "30s",
        "minTimeBetweenPriceUpdates": "5s",
        "mode": "real",
        "price": {
          "strategy": "maker"
        },
        "timeoutAction": "cancel"
      },
      "to": {
        "amount": "3",
        "currency": "ETH",
        "isPercentage": false
      }
    },
    "req": "Buy"
  },
  "out": {
    "binance": {
      "testuser1": {
        "from": "4500.67",
        "to": "3"
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterSmartBuyTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printBuyTrades(tradedAmountsPerExchange, endAmount, tradeOptions);
  expectNoStr();
}

class QueryResultPrinterSmartSellTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount startAmount{"0.15BTC"};
  TradeOptions tradeOptions;
  bool isPercentageTrade{false};
  TradedAmountsPerExchange tradedAmountsPerExchange{
      {&exchange1, TradedAmounts{MonetaryAmount("0.01BTC"), MonetaryAmount("1500USDT")}},
      {&exchange3, TradedAmounts{MonetaryAmount("0.004BTC"), MonetaryAmount("350EUR")}},
      {&exchange4, TradedAmounts{MonetaryAmount("0.1BTC"), MonetaryAmount("17ETH")}}};
};

TEST_F(QueryResultPrinterSmartSellTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printSellTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, tradeOptions);
  static constexpr std::string_view kExpected = R"(
------------------------------------------------------------------------------
| Exchange | Account   | Traded from amount (real) | Traded to amount (real) |
------------------------------------------------------------------------------
| binance  | testuser1 | 0.01 BTC                  | 1500 USDT               |
| huobi    | testuser1 | 0.004 BTC                 | 350 EUR                 |
| huobi    | testuser2 | 0.1 BTC                   | 17 ETH                  |
------------------------------------------------------------------------------
)";

  expectStr(kExpected);
}

TEST_F(QueryResultPrinterSmartSellTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson)
      .printSellTrades(TradedAmountsPerExchange{}, startAmount, isPercentageTrade, tradeOptions);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "from": {
        "amount": "0.15",
        "currency": "BTC",
        "isPercentage": false
      },
      "options": {
        "maxTradeTime": "30s",
        "minTimeBetweenPriceUpdates": "5s",
        "mode": "real",
        "price": {
          "strategy": "maker"
        },
        "timeoutAction": "cancel"
      }
    },
    "req": "Sell"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterSmartSellTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson)
      .printSellTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, tradeOptions);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "from": {
        "amount": "0.15",
        "currency": "BTC",
        "isPercentage": false
      },
      "options": {
        "maxTradeTime": "30s",
        "minTimeBetweenPriceUpdates": "5s",
        "mode": "real",
        "price": {
          "strategy": "maker"
        },
        "timeoutAction": "cancel"
      }
    },
    "req": "Sell"
  },
  "out": {
    "binance": {
      "testuser1": {
        "from": "0.01",
        "to": "1500"
      }
    },
    "huobi": {
      "testuser1": {
        "from": "0.004",
        "to": "350"
      },
      "testuser2": {
        "from": "0.1",
        "to": "17"
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterSmartSellTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint)
      .printSellTrades(tradedAmountsPerExchange, startAmount, isPercentageTrade, tradeOptions);
  expectNoStr();
}

class QueryResultPrinterOpenedOrdersBaseTest : public QueryResultPrinterTest {
 protected:
  Order order1{"id1", MonetaryAmount(0, "BTC"), MonetaryAmount(1, "BTC"), MonetaryAmount(50000, "EUR"),
               tp1,   TradeSide::kBuy};
  Order order2{"id2", MonetaryAmount("0.56ETH"), MonetaryAmount("0.44ETH"), MonetaryAmount("1500.56USDT"),
               tp2,   TradeSide::kSell};
  Order order3{"id3",          MonetaryAmount(13, "XRP"), MonetaryAmount("500.45XRP"), MonetaryAmount("1.31USDT"), tp3,
               TradeSide::kBuy};
  Order order4{"id4",           MonetaryAmount("34.56LTC"), MonetaryAmount("0.4LTC"), MonetaryAmount("1574564KRW"), tp4,
               TradeSide::kSell};
  Order order5{"id5",
               MonetaryAmount("11235435435SHIB"),
               MonetaryAmount("11235435.59SHIB"),
               MonetaryAmount("0.00000045USDT"),
               tp2,
               TradeSide::kSell};
};

class QueryResultPrinterOpenedOrdersNoConstraintsTest : public QueryResultPrinterOpenedOrdersBaseTest {
 protected:
  OrdersConstraints ordersConstraints;
  OpenedOrdersPerExchange openedOrdersPerExchange{{&exchange1, OrdersSet{}},
                                                  {&exchange2, OrdersSet{order3, order5}},
                                                  {&exchange4, OrdersSet{order2}},
                                                  {&exchange3, OrdersSet{order4, order1}}};
};

TEST_F(QueryResultPrinterOpenedOrdersNoConstraintsTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printOpenedOrders(openedOrdersPerExchange, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
---------------------------------------------------------------------------------------------------------------------------
| Exchange | Account   | Exchange Id | Placed time         | Side | Price           | Matched Amount   | Remaining Amount |
---------------------------------------------------------------------------------------------------------------------------
| bithumb  | testuser1 | id5         | 2002-06-23 07:58:35 | Sell | 0.00000045 USDT | 11235435435 SHIB | 11235435.59 SHIB |
| bithumb  | testuser1 | id3         | 2006-07-14 23:58:24 | Buy  | 1.31 USDT       | 13 XRP           | 500.45 XRP       |
| huobi    | testuser2 | id2         | 2002-06-23 07:58:35 | Sell | 1500.56 USDT    | 0.56 ETH         | 0.44 ETH         |
| huobi    | testuser1 | id1         | 1999-03-25 04:46:43 | Buy  | 50000 EUR       | 0 BTC            | 1 BTC            |
| huobi    | testuser1 | id4         | 2011-10-03 06:49:36 | Sell | 1574564 KRW     | 34.56 LTC        | 0.4 LTC          |
---------------------------------------------------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterOpenedOrdersNoConstraintsTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printOpenedOrders(OpenedOrdersPerExchange{}, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "OrdersOpened"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterOpenedOrdersNoConstraintsTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printOpenedOrders(openedOrdersPerExchange, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "OrdersOpened"
  },
  "out": {
    "binance": {
      "testuser1": []
    },
    "bithumb": {
      "testuser1": [
        {
          "id": "id5",
          "matched": "11235435435",
          "pair": "SHIB-USDT",
          "placedTime": "2002-06-23 07:58:35",
          "price": "0.00000045",
          "remaining": "11235435.59",
          "side": "Sell"
        },
        {
          "id": "id3",
          "matched": "13",
          "pair": "XRP-USDT",
          "placedTime": "2006-07-14 23:58:24",
          "price": "1.31",
          "remaining": "500.45",
          "side": "Buy"
        }
      ]
    },
    "huobi": {
      "testuser1": [
        {
          "id": "id1",
          "matched": "0",
          "pair": "BTC-EUR",
          "placedTime": "1999-03-25 04:46:43",
          "price": "50000",
          "remaining": "1",
          "side": "Buy"
        },
        {
          "id": "id4",
          "matched": "34.56",
          "pair": "LTC-KRW",
          "placedTime": "2011-10-03 06:49:36",
          "price": "1574564",
          "remaining": "0.4",
          "side": "Sell"
        }
      ],
      "testuser2": [
        {
          "id": "id2",
          "matched": "0.56",
          "pair": "ETH-USDT",
          "placedTime": "2002-06-23 07:58:35",
          "price": "1500.56",
          "remaining": "0.44",
          "side": "Sell"
        }
      ]
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterOpenedOrdersNoConstraintsTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printOpenedOrders(openedOrdersPerExchange, ordersConstraints);
  expectNoStr();
}

class QueryResultPrinterRecentDepositsBaseTest : public QueryResultPrinterTest {
 protected:
  Deposit deposit1{"id1", tp1, MonetaryAmount("0.045", "BTC")};
  Deposit deposit2{"id2", tp2, MonetaryAmount(37, "XRP")};
  Deposit deposit3{"id3", tp3, MonetaryAmount("15020.67", "EUR")};
  Deposit deposit4{"id4", tp4, MonetaryAmount("1.31", "ETH")};
  Deposit deposit5{"id5", tp4, MonetaryAmount("69204866.9", "DOGE")};
};

class QueryResultPrinterRecentDepositsNoConstraintsTest : public QueryResultPrinterRecentDepositsBaseTest {
 protected:
  DepositsConstraints depositsConstraints;
  DepositsPerExchange depositsPerExchange{{&exchange1, DepositsSet{}},
                                          {&exchange2, DepositsSet{deposit3, deposit5}},
                                          {&exchange4, DepositsSet{deposit2}},
                                          {&exchange3, DepositsSet{deposit4, deposit1}}};
};

TEST_F(QueryResultPrinterRecentDepositsNoConstraintsTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printRecentDeposits(depositsPerExchange, depositsConstraints);
  static constexpr std::string_view kExpected = R"(
------------------------------------------------------------------------------
| Exchange | Account   | Exchange Id | Received time       | Amount          |
------------------------------------------------------------------------------
| bithumb  | testuser1 | id3         | 2006-07-14 23:58:24 | 15020.67 EUR    |
| bithumb  | testuser1 | id5         | 2011-10-03 06:49:36 | 69204866.9 DOGE |
| huobi    | testuser2 | id2         | 2002-06-23 07:58:35 | 37 XRP          |
| huobi    | testuser1 | id1         | 1999-03-25 04:46:43 | 0.045 BTC       |
| huobi    | testuser1 | id4         | 2011-10-03 06:49:36 | 1.31 ETH        |
------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterRecentDepositsNoConstraintsTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printRecentDeposits(DepositsPerExchange{}, depositsConstraints);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "RecentDeposits"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterRecentDepositsNoConstraintsTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printRecentDeposits(depositsPerExchange, depositsConstraints);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "RecentDeposits"
  },
  "out": {
    "binance": {
      "testuser1": []
    },
    "bithumb": {
      "testuser1": [
        {
          "amount": "15020.67",
          "cur": "EUR",
          "id": "id3",
          "receivedTime": "2006-07-14 23:58:24"
        },
        {
          "amount": "69204866.9",
          "cur": "DOGE",
          "id": "id5",
          "receivedTime": "2011-10-03 06:49:36"
        }
      ]
    },
    "huobi": {
      "testuser1": [
        {
          "amount": "0.045",
          "cur": "BTC",
          "id": "id1",
          "receivedTime": "1999-03-25 04:46:43"
        },
        {
          "amount": "1.31",
          "cur": "ETH",
          "id": "id4",
          "receivedTime": "2011-10-03 06:49:36"
        }
      ],
      "testuser2": [
        {
          "amount": "37",
          "cur": "XRP",
          "id": "id2",
          "receivedTime": "2002-06-23 07:58:35"
        }
      ]
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterRecentDepositsNoConstraintsTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printRecentDeposits(depositsPerExchange, depositsConstraints);
  expectNoStr();
}

class QueryResultPrinterCancelOrdersTest : public QueryResultPrinterTest {
 protected:
  OrdersConstraints ordersConstraints;
  NbCancelledOrdersPerExchange nbCancelledOrdersPerExchange{
      {&exchange1, 2}, {&exchange2, 3}, {&exchange4, 1}, {&exchange3, 17}};
};

TEST_F(QueryResultPrinterCancelOrdersTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printCancelledOrders(nbCancelledOrdersPerExchange, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
-----------------------------------------------------
| Exchange | Account   | Number of cancelled orders |
-----------------------------------------------------
| binance  | testuser1 | 2                          |
| bithumb  | testuser1 | 3                          |
| huobi    | testuser2 | 1                          |
| huobi    | testuser1 | 17                         |
-----------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterCancelOrdersTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printCancelledOrders(NbCancelledOrdersPerExchange{}, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "OrdersCancel"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterCancelOrdersTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printCancelledOrders(nbCancelledOrdersPerExchange, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "OrdersCancel"
  },
  "out": {
    "binance": {
      "testuser1": {
        "nb": 2
      }
    },
    "bithumb": {
      "testuser1": {
        "nb": 3
      }
    },
    "huobi": {
      "testuser1": {
        "nb": 17
      },
      "testuser2": {
        "nb": 1
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterCancelOrdersTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printCancelledOrders(nbCancelledOrdersPerExchange, ordersConstraints);
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
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printConversionPath(marketForPath, conversionPathPerExchange);
  static constexpr std::string_view kExpected = R"(
--------------------------------------------------
| Exchange | Fastest conversion path for XLM-XRP |
--------------------------------------------------
| bithumb  | XLM-XRP                             |
| huobi    | XLM-AAA,BBB-AAA,BBB-XRP             |
--------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterConversionPathTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printConversionPath(marketForPath, ConversionPathPerExchange{});
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
  QueryResultPrinter(ss, ApiOutputType::kJson).printConversionPath(marketForPath, conversionPathPerExchange);
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
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printConversionPath(marketForPath, conversionPathPerExchange);
  expectNoStr();
}

class QueryResultPrinterWithdrawFeeTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode curWithdrawFee{"ETH"};
  MonetaryAmountPerExchange withdrawFeePerExchange{{&exchange2, MonetaryAmount{"0.15", "ETH"}},
                                                   {&exchange4, MonetaryAmount{"0.05", "ETH"}}};
};

TEST_F(QueryResultPrinterWithdrawFeeTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printWithdrawFees(withdrawFeePerExchange, curWithdrawFee);
  static constexpr std::string_view kExpected = R"(
---------------------------
| Exchange | Withdraw fee |
---------------------------
| bithumb  | 0.15 ETH     |
| huobi    | 0.05 ETH     |
---------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterWithdrawFeeTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printWithdrawFees(MonetaryAmountPerExchange{}, curWithdrawFee);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "ETH"
    },
    "req": "WithdrawFee"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterWithdrawFeeTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printWithdrawFees(withdrawFeePerExchange, curWithdrawFee);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "ETH"
    },
    "req": "WithdrawFee"
  },
  "out": {
    "bithumb": "0.15",
    "huobi": "0.05"
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterWithdrawFeeTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printWithdrawFees(withdrawFeePerExchange, curWithdrawFee);
  expectNoStr();
}

class QueryResultPrinterLast24HoursTradedVolumeTest : public QueryResultPrinterTest {
 protected:
  Market marketLast24hTradedVolume{"BTC", "EUR"};
  MonetaryAmountPerExchange monetaryAmountPerExchange{{&exchange1, MonetaryAmount{"37.8", "BTC"}},
                                                      {&exchange3, MonetaryAmount{"14", "BTC"}}};
};

TEST_F(QueryResultPrinterLast24HoursTradedVolumeTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printLast24hTradedVolume(marketLast24hTradedVolume, monetaryAmountPerExchange);
  static constexpr std::string_view kExpected = R"(
---------------------------------------------
| Exchange | Last 24h BTC-EUR traded volume |
---------------------------------------------
| binance  | 37.8 BTC                       |
| huobi    | 14 BTC                         |
---------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterLast24HoursTradedVolumeTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson)
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
  QueryResultPrinter(ss, ApiOutputType::kJson)
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
  QueryResultPrinter(ss, ApiOutputType::kNoPrint)
      .printLast24hTradedVolume(marketLast24hTradedVolume, monetaryAmountPerExchange);
  expectNoStr();
}

class QueryResultPrinterLastTradesVolumeTest : public QueryResultPrinterTest {
 protected:
  Market marketLastTrades{"ETH", "USDT"};
  int nbLastTrades = 3;
  LastTradesPerExchange lastTradesPerExchange{
      {&exchange1,
       LastTradesVector{
           PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp1),
           PublicTrade(TradeSide::kSell, MonetaryAmount{"3.7", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp2),
           PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.004", "ETH"}, MonetaryAmount{1501, "USDT"}, tp3)}},
      {&exchange3,
       LastTradesVector{
           PublicTrade(TradeSide::kSell, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp4),
           PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.004", "ETH"}, MonetaryAmount{1501, "USDT"}, tp2)}},
      {&exchange2,
       LastTradesVector{
           PublicTrade(TradeSide::kSell, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp4),
           PublicTrade(TradeSide::kBuy, MonetaryAmount{"0.004", "ETH"}, MonetaryAmount{1501, "USDT"}, tp2),
           PublicTrade(TradeSide::kBuy, MonetaryAmount{"47.78", "ETH"}, MonetaryAmount{1498, "USDT"}, tp1)}}};
};

TEST_F(QueryResultPrinterLastTradesVolumeTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printLastTrades(marketLastTrades, nbLastTrades, lastTradesPerExchange);
  static constexpr std::string_view kExpected = R"(
--------------------------------------------------------------------------------------------
| binance trades - UTC | ETH buys           | Price in USDT            | ETH sells         |
--------------------------------------------------------------------------------------------
| 1999-03-25 04:46:43  | 0.13               | 1500.5                   |                   |
| 2002-06-23 07:58:35  |                    | 1500.5                   | 3.7               |
| 2006-07-14 23:58:24  | 0.004              | 1501                     |                   |
--------------------------------------------------------------------------------------------
| Summary              | 0.134 ETH (2 buys) | 1500.66666666666666 USDT | 3.7 ETH (1 sells) |
--------------------------------------------------------------------------------------------
---------------------------------------------------------------------------------
| huobi trades - UTC  | ETH buys           | Price in USDT | ETH sells          |
---------------------------------------------------------------------------------
| 2011-10-03 06:49:36 |                    | 1500.5        | 0.13               |
| 2002-06-23 07:58:35 | 0.004              | 1501          |                    |
---------------------------------------------------------------------------------
| Summary             | 0.004 ETH (1 buys) | 1500.75 USDT  | 0.13 ETH (1 sells) |
---------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------
| bithumb trades - UTC | ETH buys            | Price in USDT            | ETH sells          |
----------------------------------------------------------------------------------------------
| 2011-10-03 06:49:36  |                     | 1500.5                   | 0.13               |
| 2002-06-23 07:58:35  | 0.004               | 1501                     |                    |
| 1999-03-25 04:46:43  | 47.78               | 1498                     |                    |
----------------------------------------------------------------------------------------------
| Summary              | 47.784 ETH (2 buys) | 1499.83333333333333 USDT | 0.13 ETH (1 sells) |
----------------------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterLastTradesVolumeTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printLastTrades(marketLastTrades, nbLastTrades, LastTradesPerExchange{});
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
  QueryResultPrinter(ss, ApiOutputType::kJson).printLastTrades(marketLastTrades, nbLastTrades, lastTradesPerExchange);
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
        "time": "1999-03-25 04:46:43"
      },
      {
        "a": "3.7",
        "p": "1500.5",
        "side": "Sell",
        "time": "2002-06-23 07:58:35"
      },
      {
        "a": "0.004",
        "p": "1501",
        "side": "Buy",
        "time": "2006-07-14 23:58:24"
      }
    ],
    "bithumb": [
      {
        "a": "0.13",
        "p": "1500.5",
        "side": "Sell",
        "time": "2011-10-03 06:49:36"
      },
      {
        "a": "0.004",
        "p": "1501",
        "side": "Buy",
        "time": "2002-06-23 07:58:35"
      },
      {
        "a": "47.78",
        "p": "1498",
        "side": "Buy",
        "time": "1999-03-25 04:46:43"
      }
    ],
    "huobi": [
      {
        "a": "0.13",
        "p": "1500.5",
        "side": "Sell",
        "time": "2011-10-03 06:49:36"
      },
      {
        "a": "0.004",
        "p": "1501",
        "side": "Buy",
        "time": "2002-06-23 07:58:35"
      }
    ]
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterLastTradesVolumeTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint)
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
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable).printLastPrice(marketLastPrice, monetaryAmountPerExchange);
  static constexpr std::string_view kExpected = R"(
---------------------------------
| Exchange | XRP-KRW last price |
---------------------------------
| binance  | 417 KRW            |
| huobi    | 444 KRW            |
| bithumb  | 590 KRW            |
---------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterLastPriceTest, EmptyJson) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printLastPrice(marketLastPrice, MonetaryAmountPerExchange{});
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
  QueryResultPrinter(ss, ApiOutputType::kJson).printLastPrice(marketLastPrice, monetaryAmountPerExchange);
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
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printLastPrice(marketLastPrice, monetaryAmountPerExchange);
  expectNoStr();
}

class QueryResultPrinterWithdrawTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount grossAmount{"76.55 XRP"};
  MonetaryAmount netEmittedAmount{"75.55 XRP"};
  MonetaryAmount fee = grossAmount - netEmittedAmount;
  bool isWithdrawSent = true;
  ExchangeName fromExchange{exchange1.apiPrivate().exchangeName()};
  ExchangeName toExchange{exchange4.apiPrivate().exchangeName()};

  Wallet receivingWallet{toExchange, grossAmount.currencyCode(), "xrpaddress666", "xrptag2", WalletCheck{}};
  std::string_view withdrawId = "WithdrawTest01";
  MonetaryAmount grossEmittedAmount;
  api::InitiatedWithdrawInfo initiatedWithdrawInfo{receivingWallet, withdrawId, grossAmount, tp1};
  api::SentWithdrawInfo sentWithdrawInfo{netEmittedAmount, fee, isWithdrawSent};
  WithdrawInfo withdrawInfo{initiatedWithdrawInfo, netEmittedAmount, tp2};
};

class QueryResultPrinterWithdrawAmountTest : public QueryResultPrinterWithdrawTest {
 protected:
  bool isPercentageWithdraw = false;
};

TEST_F(QueryResultPrinterWithdrawAmountTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printWithdraw(withdrawInfo, grossAmount, isPercentageWithdraw, fromExchange, toExchange);
  static constexpr std::string_view kExpected = R"(
-------------------------------------------------------------------------------------------------------------------------
| From Exchange | To Exchange | Gross withdraw amount | Initiated time      | Received time       | Net received amount |
-------------------------------------------------------------------------------------------------------------------------
| binance       | huobi       | 76.55 XRP             | 1999-03-25 04:46:43 | 2002-06-23 07:58:35 | 75.55 XRP           |
-------------------------------------------------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterWithdrawAmountTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson)
      .printWithdraw(withdrawInfo, grossAmount, isPercentageWithdraw, fromExchange, toExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "XRP",
      "grossAmount": "76.55",
      "isPercentage": false
    },
    "req": "Withdraw"
  },
  "out": {
    "from": {
      "account": "testuser1",
      "exchange": "binance"
    },
    "initiatedTime": "1999-03-25 04:46:43",
    "netReceivedAmount": "75.55",
    "receivedTime": "2002-06-23 07:58:35",
    "to": {
      "account": "testuser2",
      "address": "xrpaddress666",
      "exchange": "huobi",
      "tag": "xrptag2"
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterWithdrawAmountTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint)
      .printWithdraw(withdrawInfo, grossAmount, isPercentageWithdraw, fromExchange, toExchange);
  expectNoStr();
}

class QueryResultPrinterWithdrawPercentageTest : public QueryResultPrinterWithdrawTest {
 protected:
  bool isPercentageWithdraw = true;
};

TEST_F(QueryResultPrinterWithdrawPercentageTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printWithdraw(withdrawInfo, grossAmount, isPercentageWithdraw, fromExchange, toExchange);
  static constexpr std::string_view kExpected = R"(
-------------------------------------------------------------------------------------------------------------------------
| From Exchange | To Exchange | Gross withdraw amount | Initiated time      | Received time       | Net received amount |
-------------------------------------------------------------------------------------------------------------------------
| binance       | huobi       | 76.55 XRP             | 1999-03-25 04:46:43 | 2002-06-23 07:58:35 | 75.55 XRP           |
-------------------------------------------------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterWithdrawPercentageTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson)
      .printWithdraw(withdrawInfo, grossAmount, isPercentageWithdraw, fromExchange, toExchange);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "XRP",
      "grossAmount": "76.55",
      "isPercentage": true
    },
    "req": "Withdraw"
  },
  "out": {
    "from": {
      "account": "testuser1",
      "exchange": "binance"
    },
    "initiatedTime": "1999-03-25 04:46:43",
    "netReceivedAmount": "75.55",
    "receivedTime": "2002-06-23 07:58:35",
    "to": {
      "account": "testuser2",
      "address": "xrpaddress666",
      "exchange": "huobi",
      "tag": "xrptag2"
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterWithdrawPercentageTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint)
      .printWithdraw(withdrawInfo, grossAmount, isPercentageWithdraw, fromExchange, toExchange);
  expectNoStr();
}

class QueryResultPrinterDustSweeperTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode cur{"ETH"};
  CurrencyCode cur1{"BTC"};
  CurrencyCode cur2{"EUR"};
  TradedAmountsVectorWithFinalAmountPerExchange tradedAmountsVectorWithFinalAmountPerExchange{
      {&exchange1,
       {TradedAmountsVector{TradedAmounts{MonetaryAmount{9847, cur, 2}, MonetaryAmount{"0.00005", cur1}}},
        MonetaryAmount{0, cur}}},
      {&exchange3, {TradedAmountsVector{}, MonetaryAmount{156, cur, 2}}},
      {&exchange4,
       {TradedAmountsVector{TradedAmounts{MonetaryAmount{"0.45609", cur2}, MonetaryAmount{9847, cur, 2}},
                            TradedAmounts{MonetaryAmount{150945, cur, 2}, MonetaryAmount{"0.000612", cur1}}},
        MonetaryAmount{0, cur}}}};
};

TEST_F(QueryResultPrinterDustSweeperTest, FormattedTable) {
  QueryResultPrinter(ss, ApiOutputType::kFormattedTable)
      .printDustSweeper(tradedAmountsVectorWithFinalAmountPerExchange, cur);
  static constexpr std::string_view kExpected = R"(
-----------------------------------------------------------------------------------------------
| Exchange | Account   | Trades                                                | Final Amount |
-----------------------------------------------------------------------------------------------
| binance  | testuser1 | 98.47 ETH -> 0.00005 BTC                              | 0 ETH        |
| huobi    | testuser1 |                                                       | 1.56 ETH     |
| huobi    | testuser2 | 0.45609 EUR -> 98.47 ETH, 1509.45 ETH -> 0.000612 BTC | 0 ETH        |
-----------------------------------------------------------------------------------------------
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterDustSweeperTest, Json) {
  QueryResultPrinter(ss, ApiOutputType::kJson).printDustSweeper(tradedAmountsVectorWithFinalAmountPerExchange, cur);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "ETH"
    },
    "req": "DustSweeper"
  },
  "out": {
    "binance": {
      "testuser1": {
        "finalAmount": "0 ETH",
        "trades": [
          {
            "from": "98.47 ETH",
            "to": "0.00005 BTC"
          }
        ]
      }
    },
    "huobi": {
      "testuser1": {
        "finalAmount": "1.56 ETH",
        "trades": []
      },
      "testuser2": {
        "finalAmount": "0 ETH",
        "trades": [
          {
            "from": "0.45609 EUR",
            "to": "98.47 ETH"
          },
          {
            "from": "1509.45 ETH",
            "to": "0.000612 BTC"
          }
        ]
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterDustSweeperTest, NoPrint) {
  QueryResultPrinter(ss, ApiOutputType::kNoPrint).printDustSweeper(tradedAmountsVectorWithFinalAmountPerExchange, cur);
  expectNoStr();
}

}  // namespace cct