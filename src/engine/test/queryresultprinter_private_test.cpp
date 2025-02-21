#include <gtest/gtest.h>

#include <string_view>

#include "accountowner.hpp"
#include "apioutputtype.hpp"
#include "balanceportfolio.hpp"
#include "cct_exception.hpp"
#include "closed-order.hpp"
#include "currencycode.hpp"
#include "deposit.hpp"
#include "depositsconstraints.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapitypes.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "ordersconstraints.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
#include "queryresultprinter.hpp"
#include "queryresultprinter_base_test.hpp"
#include "queryresulttypes.hpp"
#include "tradedamounts.hpp"
#include "tradeoptions.hpp"
#include "traderesult.hpp"
#include "tradeside.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawoptions.hpp"
#include "withdrawordeposit.hpp"
#include "withdrawsconstraints.hpp"

namespace cct {

class QueryResultPrinterEmptyBalanceNoEquiCurTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode equiCur;
  BalancePortfolio emptyBal;
  BalancePerExchange balancePerExchange{{&exchange1, emptyBal}, {&exchange4, emptyBal}};
};

TEST_F(QueryResultPrinterEmptyBalanceNoEquiCurTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table).printBalance(balancePerExchange, equiCur);
  static constexpr std::string_view kExpected = R"(
+----------+--------------------------+-------------------+-----------------+
| Currency | Total amount on selected | binance_testuser1 | huobi_testuser2 |
+----------+--------------------------+-------------------+-----------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterEmptyBalanceNoEquiCurTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printBalance(BalancePerExchange{}, equiCur);
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
  basicQueryResultPrinter(ApiOutputType::json).printBalance(balancePerExchange, equiCur);
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
  basicQueryResultPrinter(ApiOutputType::off).printBalance(balancePerExchange, equiCur);
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
  basicQueryResultPrinter(ApiOutputType::table).printBalance(balancePerExchange, equiCur);
  static constexpr std::string_view kExpected = R"(
+----------+--------------------------+-------------------+-----------------+-------------------+
| Currency | Total amount on selected | binance_testuser1 | huobi_testuser2 | bithumb_testuser1 |
+----------+--------------------------+-------------------+-----------------+-------------------+
| ADA      | 147                      | 0                 | 147             | 0                 |
| BTC      | 15                       | 15                | 0               | 0                 |
| DOT      | 4.76                     | 0                 | 4.76            | 0                 |
| ETH      | 1.5                      | 1.5               | 0               | 0                 |
| EUR      | 1200                     | 0                 | 1200            | 0                 |
| MATIC    | 15004                    | 0                 | 15004           | 0                 |
| USD      | 155                      | 0                 | 155             | 0                 |
| USDT     | 5107.5                   | 5000              | 107.5           | 0                 |
| XRP      | 1500                     | 1500              | 0               | 0                 |
+----------+--------------------------+-------------------+-----------------+-------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterBalanceNoEquiCurTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printBalance(BalancePerExchange{}, equiCur);
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
  basicQueryResultPrinter(ApiOutputType::json).printBalance(balancePerExchange, equiCur);
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
  basicQueryResultPrinter(ApiOutputType::off).printBalance(balancePerExchange, equiCur);
  expectNoStr();
}

class QueryResultPrinterBalanceEquiCurTest : public QueryResultPrinterTest {
 protected:
  void SetUp() override {
    for (auto &[amount, equi] : bp1) {
      if (amount == ma1) {
        equi = MonetaryAmount{10000, equiCur};
      } else if (amount == ma2) {
        equi = MonetaryAmount{90677, equiCur, 1};
      } else {
        throw exception("Should not happen");
      }
    }

    for (auto &[amount, equi] : bp2) {
      if (amount == ma3) {
        equi = MonetaryAmount{4508, equiCur, 2};
      } else if (amount == ma4) {
        equi = MonetaryAmount{25000, equiCur};
      } else if (amount == ma5) {
        equi = MonetaryAmount{675, equiCur, 1};
      } else {
        throw exception("Should not happen");
      }
    }

    balancePerExchange = BalancePerExchange{{&exchange1, bp1}, {&exchange4, bp2}, {&exchange2, bp3}, {&exchange3, bp3}};
  }

  MonetaryAmount ma1{15000, "ADA"};
  MonetaryAmount ma2{56, "BTC", 2};
  MonetaryAmount ma3{347, "XRP", 1};
  MonetaryAmount ma4{15, "ETH"};
  MonetaryAmount ma5{123, "XLM"};

  CurrencyCode equiCur{"EUR"};
  BalancePortfolio bp1{ma1, ma2};
  BalancePortfolio bp2{ma3, ma4, ma5};
  BalancePortfolio bp3;
  BalancePerExchange balancePerExchange;
};

TEST_F(QueryResultPrinterBalanceEquiCurTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table).printBalance(balancePerExchange, equiCur);
  static constexpr std::string_view kExpected = R"(
+----------+--------------------------+--------------+-------------------+-----------------+-------------------+-----------------+
| Currency | Total amount on selected | Total EUR eq | binance_testuser1 | huobi_testuser2 | bithumb_testuser1 | huobi_testuser1 |
+----------+--------------------------+--------------+-------------------+-----------------+-------------------+-----------------+
| ETH      | 15                       | 25000        | 0                 | 15              | 0                 | 0               |
| ADA      | 15000                    | 10000        | 15000             | 0               | 0                 | 0               |
| BTC      | 0.56                     | 9067.7       | 0.56              | 0               | 0                 | 0               |
| XLM      | 123                      | 67.5         | 0                 | 123             | 0                 | 0               |
| XRP      | 34.7                     | 45.08        | 0                 | 34.7            | 0                 | 0               |
+----------+--------------------------+--------------+-------------------+-----------------+-------------------+-----------------+
| Total    |                          | 44180.28     |                   |                 |                   |                 |
+----------+--------------------------+--------------+-------------------+-----------------+-------------------+-----------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterBalanceEquiCurTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printBalance(BalancePerExchange{}, equiCur);
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
  basicQueryResultPrinter(ApiOutputType::json).printBalance(balancePerExchange, equiCur);
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
  basicQueryResultPrinter(ApiOutputType::off).printBalance(balancePerExchange, equiCur);
  expectNoStr();
}

class QueryResultPrinterDepositInfoWithoutTagTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode depositCurrencyCode{"ETH"};
  WalletPerExchange walletPerExchange{
      {&exchange2, Wallet{exchange2.apiPrivate().exchangeName(), depositCurrencyCode, "ethaddress666", "",
                          WalletCheck{}, AccountOwner("SmithJohn", "스미스존")}},
      {&exchange4, Wallet{exchange4.apiPrivate().exchangeName(), depositCurrencyCode, "ethaddress667", "",
                          WalletCheck{}, AccountOwner("GilbertDave", "길버트데이브")}}};
};

TEST_F(QueryResultPrinterDepositInfoWithoutTagTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table).printDepositInfo(depositCurrencyCode, walletPerExchange);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+---------------+-----------------+
| Exchange | Account   | ETH address   | Destination Tag |
+----------+-----------+---------------+-----------------+
| bithumb  | testuser1 | ethaddress666 |                 |
| huobi    | testuser2 | ethaddress667 |                 |
+----------+-----------+---------------+-----------------+
)";

  expectStr(kExpected);
}

TEST_F(QueryResultPrinterDepositInfoWithoutTagTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printDepositInfo(depositCurrencyCode, WalletPerExchange{});
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
  basicQueryResultPrinter(ApiOutputType::json).printDepositInfo(depositCurrencyCode, walletPerExchange);
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
  basicQueryResultPrinter(ApiOutputType::off).printDepositInfo(depositCurrencyCode, walletPerExchange);
  expectNoStr();
}

class QueryResultPrinterDepositInfoWithTagTest : public QueryResultPrinterTest {
 protected:
  CurrencyCode depositCurrencyCode{"XRP"};
  WalletPerExchange walletPerExchange{
      {&exchange3, Wallet{exchange3.apiPrivate().exchangeName(), depositCurrencyCode, "xrpaddress666", "xrptag1",
                          WalletCheck{}, AccountOwner("SmithJohn", "스미스존")}},
      {&exchange4, Wallet{exchange4.apiPrivate().exchangeName(), depositCurrencyCode, "xrpaddress666", "xrptag2",
                          WalletCheck{}, AccountOwner("GilbertDave", "길버트데이브")}}};
};

TEST_F(QueryResultPrinterDepositInfoWithTagTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table).printDepositInfo(depositCurrencyCode, walletPerExchange);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+---------------+-----------------+
| Exchange | Account   | XRP address   | Destination Tag |
+----------+-----------+---------------+-----------------+
| huobi    | testuser1 | xrpaddress666 | xrptag1         |
| huobi    | testuser2 | xrpaddress666 | xrptag2         |
+----------+-----------+---------------+-----------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterDepositInfoWithTagTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printDepositInfo(depositCurrencyCode, WalletPerExchange{});
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
  basicQueryResultPrinter(ApiOutputType::json).printDepositInfo(depositCurrencyCode, walletPerExchange);
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
  basicQueryResultPrinter(ApiOutputType::off).printDepositInfo(depositCurrencyCode, walletPerExchange);
  expectNoStr();
}

class QueryResultPrinterTradesAmountTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount startAmount{"0.5BTC"};
  bool isPercentageTrade{false};
  CurrencyCode toCurrency{"XRP"};
  TradedAmounts tradedAmounts1{MonetaryAmount("0.1BTC"), MonetaryAmount("1050XRP")};
  TradedAmounts tradedAmounts3{MonetaryAmount("0.3BTC"), MonetaryAmount("3500.6XRP")};
  TradedAmounts tradedAmounts4{MonetaryAmount(0, "BTC"), MonetaryAmount(0, "XRP")};

  TradeResultPerExchange tradeResultPerExchange{{&exchange1, TradeResult{tradedAmounts1, tradedAmounts1.from}},
                                                {&exchange3, TradeResult{tradedAmounts3, tradedAmounts3.from * 2}},
                                                {&exchange4, TradeResult{tradedAmounts4, MonetaryAmount(1, "BTC")}}};
};

TEST_F(QueryResultPrinterTradesAmountTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table)
      .printTrades(tradeResultPerExchange, startAmount, isPercentageTrade, toCurrency, defaultTradeOptions);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+---------+---------------------------+-------------------------+-----------+
| Exchange | Account   | From    | Traded from amount (real) | Traded to amount (real) | Status    |
+----------+-----------+---------+---------------------------+-------------------------+-----------+
| binance  | testuser1 | 0.1 BTC | 0.1 BTC                   | 1050 XRP                | complete  |
| huobi    | testuser1 | 0.6 BTC | 0.3 BTC                   | 3500.6 XRP              | partial   |
| huobi    | testuser2 | 1 BTC   | 0 BTC                     | 0 XRP                   | untouched |
+----------+-----------+---------+---------------------------+-------------------------+-----------+
)";

  expectStr(kExpected);
}

TEST_F(QueryResultPrinterTradesAmountTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json)
      .printTrades(TradeResultPerExchange{}, startAmount, isPercentageTrade, toCurrency, defaultTradeOptions);
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
        "syncPolicy": "synchronous",
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
  basicQueryResultPrinter(ApiOutputType::json)
      .printTrades(tradeResultPerExchange, startAmount, isPercentageTrade, toCurrency, defaultTradeOptions);
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
        "syncPolicy": "synchronous",
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
        "status": "complete",
        "tradedFrom": "0.1",
        "tradedTo": "1050"
      }
    },
    "huobi": {
      "testuser1": {
        "from": "0.6",
        "status": "partial",
        "tradedFrom": "0.3",
        "tradedTo": "3500.6"
      },
      "testuser2": {
        "from": "1",
        "status": "untouched",
        "tradedFrom": "0",
        "tradedTo": "0"
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterTradesAmountTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::off)
      .printTrades(tradeResultPerExchange, startAmount, isPercentageTrade, toCurrency, defaultTradeOptions);
  expectNoStr();
}

class QueryResultPrinterTradesPercentageTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount startAmount{"25.6EUR"};
  bool isPercentageTrade{true};
  CurrencyCode toCurrency{"SHIB"};
  TradeOptions tradeOptions{TradeOptions{PriceOptions{PriceStrategy::taker}},
                            coincenterInfo.exchangeConfig(exchangePublic1.exchangeNameEnum()).query.trade};
  TradedAmounts tradedAmounts{MonetaryAmount("15000.56EUR"), MonetaryAmount("885475102SHIB")};
  TradeResultPerExchange tradeResultPerExchange{{&exchange2, TradeResult{tradedAmounts, tradedAmounts.from * 2}}};
};

TEST_F(QueryResultPrinterTradesPercentageTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table)
      .printTrades(tradeResultPerExchange, startAmount, isPercentageTrade, toCurrency, defaultTradeOptions);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+--------------+---------------------------+-------------------------+---------+
| Exchange | Account   | From         | Traded from amount (real) | Traded to amount (real) | Status  |
+----------+-----------+--------------+---------------------------+-------------------------+---------+
| bithumb  | testuser1 | 30001.12 EUR | 15000.56 EUR              | 885475102 SHIB          | partial |
+----------+-----------+--------------+---------------------------+-------------------------+---------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterTradesPercentageTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json)
      .printTrades(TradeResultPerExchange{}, startAmount, isPercentageTrade, toCurrency, tradeOptions);
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
        "syncPolicy": "synchronous",
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
  basicQueryResultPrinter(ApiOutputType::json)
      .printTrades(tradeResultPerExchange, startAmount, isPercentageTrade, toCurrency, tradeOptions);
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
        "syncPolicy": "synchronous",
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
        "from": "30001.12",
        "status": "partial",
        "tradedFrom": "15000.56",
        "tradedTo": "885475102"
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterTradesPercentageTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::off)
      .printTrades(tradeResultPerExchange, startAmount, isPercentageTrade, toCurrency, tradeOptions);
  expectNoStr();
}

class QueryResultPrinterSmartBuyTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount endAmount{"3ETH"};
  TradedAmounts tradedAmounts{MonetaryAmount("4500.67EUR"), MonetaryAmount("3ETH")};
  TradeResultPerExchange tradeResultPerExchange{{&exchange1, TradeResult{tradedAmounts, tradedAmounts.from}}};
};

TEST_F(QueryResultPrinterSmartBuyTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table).printBuyTrades(tradeResultPerExchange, endAmount, defaultTradeOptions);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+-------------+---------------------------+-------------------------+----------+
| Exchange | Account   | From        | Traded from amount (real) | Traded to amount (real) | Status   |
+----------+-----------+-------------+---------------------------+-------------------------+----------+
| binance  | testuser1 | 4500.67 EUR | 4500.67 EUR               | 3 ETH                   | complete |
+----------+-----------+-------------+---------------------------+-------------------------+----------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterSmartBuyTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printBuyTrades(TradeResultPerExchange{}, endAmount, defaultTradeOptions);
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
        "syncPolicy": "synchronous",
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
  basicQueryResultPrinter(ApiOutputType::json).printBuyTrades(tradeResultPerExchange, endAmount, defaultTradeOptions);
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
        "syncPolicy": "synchronous",
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
        "status": "complete",
        "tradedFrom": "4500.67",
        "tradedTo": "3"
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterSmartBuyTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::off).printBuyTrades(tradeResultPerExchange, endAmount, defaultTradeOptions);
  expectNoStr();
}

class QueryResultPrinterSmartSellTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount startAmount{"0.15BTC"};
  bool isPercentageTrade{false};
  TradedAmounts tradedAmounts1{MonetaryAmount("0.01BTC"), MonetaryAmount("1500USDT")};
  TradedAmounts tradedAmounts3{MonetaryAmount("0.004BTC"), MonetaryAmount("350EUR")};
  TradedAmounts tradedAmounts4{MonetaryAmount("0.1BTC"), MonetaryAmount("17ETH")};
  TradeResultPerExchange tradeResultPerExchange{{&exchange1, TradeResult{tradedAmounts1, tradedAmounts1.from}},
                                                {&exchange3, TradeResult{tradedAmounts3, tradedAmounts1.from * 2}},
                                                {&exchange4, TradeResult{tradedAmounts4, tradedAmounts4.from * 3}}};
};

TEST_F(QueryResultPrinterSmartSellTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table)
      .printSellTrades(tradeResultPerExchange, startAmount, isPercentageTrade, defaultTradeOptions);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+----------+---------------------------+-------------------------+----------+
| Exchange | Account   | From     | Traded from amount (real) | Traded to amount (real) | Status   |
+----------+-----------+----------+---------------------------+-------------------------+----------+
| binance  | testuser1 | 0.01 BTC | 0.01 BTC                  | 1500 USDT               | complete |
| huobi    | testuser1 | 0.02 BTC | 0.004 BTC                 | 350 EUR                 | partial  |
| huobi    | testuser2 | 0.3 BTC  | 0.1 BTC                   | 17 ETH                  | partial  |
+----------+-----------+----------+---------------------------+-------------------------+----------+
)";

  expectStr(kExpected);
}

TEST_F(QueryResultPrinterSmartSellTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json)
      .printSellTrades(TradeResultPerExchange{}, startAmount, isPercentageTrade, defaultTradeOptions);
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
        "syncPolicy": "synchronous",
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
  basicQueryResultPrinter(ApiOutputType::json)
      .printSellTrades(tradeResultPerExchange, startAmount, isPercentageTrade, defaultTradeOptions);
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
        "syncPolicy": "synchronous",
        "timeoutAction": "cancel"
      }
    },
    "req": "Sell"
  },
  "out": {
    "binance": {
      "testuser1": {
        "from": "0.01",
        "status": "complete",
        "tradedFrom": "0.01",
        "tradedTo": "1500"
      }
    },
    "huobi": {
      "testuser1": {
        "from": "0.02",
        "status": "partial",
        "tradedFrom": "0.004",
        "tradedTo": "350"
      },
      "testuser2": {
        "from": "0.3",
        "status": "partial",
        "tradedFrom": "0.1",
        "tradedTo": "17"
      }
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterSmartSellTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::off)
      .printSellTrades(tradeResultPerExchange, startAmount, isPercentageTrade, defaultTradeOptions);
  expectNoStr();
}

class QueryResultPrinterClosedOrdersBaseTest : public QueryResultPrinterTest {
 protected:
  ClosedOrder order1{"id1", MonetaryAmount(0, "BTC"), MonetaryAmount(50000, "EUR"), tp1, tp1, TradeSide::buy};
  ClosedOrder order2{"id2", MonetaryAmount("0.56ETH"), MonetaryAmount("1500.56USDT"), tp2, tp3, TradeSide::sell};
  ClosedOrder order3{"id3", MonetaryAmount(13, "XRP"), MonetaryAmount("1.31USDT"), tp3, tp1, TradeSide::buy};
  ClosedOrder order4{"id4", MonetaryAmount("34.56LTC"), MonetaryAmount("1574564KRW"), tp4, tp2, TradeSide::sell};
  ClosedOrder order5{"id5",          MonetaryAmount("11235435.59SHIB"), MonetaryAmount("0.00000045USDT"), tp2, tp4,
                     TradeSide::sell};
};

class QueryResultPrinterClosedOrdersNoConstraintsTest : public QueryResultPrinterClosedOrdersBaseTest {
 protected:
  OrdersConstraints ordersConstraints;
  ClosedOrdersPerExchange closedOrdersPerExchange{{&exchange1, ClosedOrderSet{}},
                                                  {&exchange2, ClosedOrderSet{order3, order5}},
                                                  {&exchange4, ClosedOrderSet{order2}},
                                                  {&exchange3, ClosedOrderSet{order4, order1}}};
};

TEST_F(QueryResultPrinterClosedOrdersNoConstraintsTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table).printClosedOrders(closedOrdersPerExchange, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+-------------+----------------------+----------------------+------+-----------------+------------------+
| Exchange | Account   | Exchange Id | Placed time          | Matched time         | Side | Price           | Matched Amount   |
+----------+-----------+-------------+----------------------+----------------------+------+-----------------+------------------+
| bithumb  | testuser1 | id5         | 2002-06-23T07:58:35Z | 2011-10-03T06:49:36Z | sell | 0.00000045 USDT | 11235435.59 SHIB |
| bithumb  | testuser1 | id3         | 2006-07-14T23:58:24Z | 1999-03-25T04:46:43Z | buy  | 1.31 USDT       | 13 XRP           |
| huobi    | testuser2 | id2         | 2002-06-23T07:58:35Z | 2006-07-14T23:58:24Z | sell | 1500.56 USDT    | 0.56 ETH         |
| huobi    | testuser1 | id1         | 1999-03-25T04:46:43Z | 1999-03-25T04:46:43Z | buy  | 50000 EUR       | 0 BTC            |
| huobi    | testuser1 | id4         | 2011-10-03T06:49:36Z | 2002-06-23T07:58:35Z | sell | 1574564 KRW     | 34.56 LTC        |
+----------+-----------+-------------+----------------------+----------------------+------+-----------------+------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterClosedOrdersNoConstraintsTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printClosedOrders(ClosedOrdersPerExchange{}, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "OrdersClosed"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterClosedOrdersNoConstraintsTest, Json) {
  basicQueryResultPrinter(ApiOutputType::json).printClosedOrders(closedOrdersPerExchange, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "OrdersClosed"
  },
  "out": {
    "binance": {
      "testuser1": []
    },
    "bithumb": {
      "testuser1": [
        {
          "id": "id5",
          "matched": "11235435.59",
          "matchedTime": "2011-10-03T06:49:36Z",
          "pair": "SHIB-USDT",
          "placedTime": "2002-06-23T07:58:35Z",
          "price": "0.00000045",
          "side": "sell"
        },
        {
          "id": "id3",
          "matched": "13",
          "matchedTime": "1999-03-25T04:46:43Z",
          "pair": "XRP-USDT",
          "placedTime": "2006-07-14T23:58:24Z",
          "price": "1.31",
          "side": "buy"
        }
      ]
    },
    "huobi": {
      "testuser1": [
        {
          "id": "id1",
          "matched": "0",
          "matchedTime": "1999-03-25T04:46:43Z",
          "pair": "BTC-EUR",
          "placedTime": "1999-03-25T04:46:43Z",
          "price": "50000",
          "side": "buy"
        },
        {
          "id": "id4",
          "matched": "34.56",
          "matchedTime": "2002-06-23T07:58:35Z",
          "pair": "LTC-KRW",
          "placedTime": "2011-10-03T06:49:36Z",
          "price": "1574564",
          "side": "sell"
        }
      ],
      "testuser2": [
        {
          "id": "id2",
          "matched": "0.56",
          "matchedTime": "2006-07-14T23:58:24Z",
          "pair": "ETH-USDT",
          "placedTime": "2002-06-23T07:58:35Z",
          "price": "1500.56",
          "side": "sell"
        }
      ]
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterClosedOrdersNoConstraintsTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::off).printClosedOrders(closedOrdersPerExchange, ordersConstraints);
  expectNoStr();
}

class QueryResultPrinterOpenedOrdersBaseTest : public QueryResultPrinterTest {
 protected:
  OpenedOrder order1{
      "id1", MonetaryAmount(0, "BTC"), MonetaryAmount(1, "BTC"), MonetaryAmount(50000, "EUR"), tp1, TradeSide::buy};
  OpenedOrder order2{
      "id2", MonetaryAmount("0.56ETH"), MonetaryAmount("0.44ETH"), MonetaryAmount("1500.56USDT"), tp2, TradeSide::sell};
  OpenedOrder order3{
      "id3", MonetaryAmount(13, "XRP"), MonetaryAmount("500.45XRP"), MonetaryAmount("1.31USDT"), tp3, TradeSide::buy};
  OpenedOrder order4{
      "id4", MonetaryAmount("34.56LTC"), MonetaryAmount("0.4LTC"), MonetaryAmount("1574564KRW"), tp4, TradeSide::sell};
  OpenedOrder order5{"id5",
                     MonetaryAmount("11235435435SHIB"),
                     MonetaryAmount("11235435.59SHIB"),
                     MonetaryAmount("0.00000045USDT"),
                     tp2,
                     TradeSide::sell};
};

class QueryResultPrinterOpenedOrdersNoConstraintsTest : public QueryResultPrinterOpenedOrdersBaseTest {
 protected:
  OrdersConstraints ordersConstraints;
  OpenedOrdersPerExchange openedOrdersPerExchange{{&exchange1, OpenedOrderSet{}},
                                                  {&exchange2, OpenedOrderSet{order3, order5}},
                                                  {&exchange4, OpenedOrderSet{order2}},
                                                  {&exchange3, OpenedOrderSet{order4, order1}}};
};

TEST_F(QueryResultPrinterOpenedOrdersNoConstraintsTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table).printOpenedOrders(openedOrdersPerExchange, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+-------------+----------------------+------+-----------------+------------------+------------------+
| Exchange | Account   | Exchange Id | Placed time          | Side | Price           | Matched Amount   | Remaining Amount |
+----------+-----------+-------------+----------------------+------+-----------------+------------------+------------------+
| bithumb  | testuser1 | id5         | 2002-06-23T07:58:35Z | sell | 0.00000045 USDT | 11235435435 SHIB | 11235435.59 SHIB |
| bithumb  | testuser1 | id3         | 2006-07-14T23:58:24Z | buy  | 1.31 USDT       | 13 XRP           | 500.45 XRP       |
| huobi    | testuser2 | id2         | 2002-06-23T07:58:35Z | sell | 1500.56 USDT    | 0.56 ETH         | 0.44 ETH         |
| huobi    | testuser1 | id1         | 1999-03-25T04:46:43Z | buy  | 50000 EUR       | 0 BTC            | 1 BTC            |
| huobi    | testuser1 | id4         | 2011-10-03T06:49:36Z | sell | 1574564 KRW     | 34.56 LTC        | 0.4 LTC          |
+----------+-----------+-------------+----------------------+------+-----------------+------------------+------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterOpenedOrdersNoConstraintsTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printOpenedOrders(OpenedOrdersPerExchange{}, ordersConstraints);
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
  basicQueryResultPrinter(ApiOutputType::json).printOpenedOrders(openedOrdersPerExchange, ordersConstraints);
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
          "placedTime": "2002-06-23T07:58:35Z",
          "price": "0.00000045",
          "remaining": "11235435.59",
          "side": "sell"
        },
        {
          "id": "id3",
          "matched": "13",
          "pair": "XRP-USDT",
          "placedTime": "2006-07-14T23:58:24Z",
          "price": "1.31",
          "remaining": "500.45",
          "side": "buy"
        }
      ]
    },
    "huobi": {
      "testuser1": [
        {
          "id": "id1",
          "matched": "0",
          "pair": "BTC-EUR",
          "placedTime": "1999-03-25T04:46:43Z",
          "price": "50000",
          "remaining": "1",
          "side": "buy"
        },
        {
          "id": "id4",
          "matched": "34.56",
          "pair": "LTC-KRW",
          "placedTime": "2011-10-03T06:49:36Z",
          "price": "1574564",
          "remaining": "0.4",
          "side": "sell"
        }
      ],
      "testuser2": [
        {
          "id": "id2",
          "matched": "0.56",
          "pair": "ETH-USDT",
          "placedTime": "2002-06-23T07:58:35Z",
          "price": "1500.56",
          "remaining": "0.44",
          "side": "sell"
        }
      ]
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterOpenedOrdersNoConstraintsTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::off).printOpenedOrders(openedOrdersPerExchange, ordersConstraints);
  expectNoStr();
}

class QueryResultPrinterRecentDepositsBaseTest : public QueryResultPrinterTest {
 protected:
  Deposit deposit1{"id1", tp1, MonetaryAmount("0.045", "BTC"), Deposit::Status::initial};
  Deposit deposit2{"id2", tp2, MonetaryAmount(37, "XRP"), Deposit::Status::success};
  Deposit deposit3{"id3", tp3, MonetaryAmount("15020.67", "EUR"), Deposit::Status::failed};
  Deposit deposit4{"id4", tp4, MonetaryAmount("1.31", "ETH"), Deposit::Status::processing};
  Deposit deposit5{"id5", tp4, MonetaryAmount("69204866.9", "DOGE"), Deposit::Status::success};
};

class QueryResultPrinterRecentDepositsNoConstraintsTest : public QueryResultPrinterRecentDepositsBaseTest {
 protected:
  DepositsConstraints constraints;
  DepositsPerExchange depositsPerExchange{{&exchange1, DepositsSet{}},
                                          {&exchange2, DepositsSet{deposit3, deposit5}},
                                          {&exchange4, DepositsSet{deposit2}},
                                          {&exchange3, DepositsSet{deposit4, deposit1}}};
};

TEST_F(QueryResultPrinterRecentDepositsNoConstraintsTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table).printRecentDeposits(depositsPerExchange, constraints);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+-------------+----------------------+-----------------+------------+
| Exchange | Account   | Exchange Id | Received time        | Amount          | Status     |
+----------+-----------+-------------+----------------------+-----------------+------------+
| bithumb  | testuser1 | id3         | 2006-07-14T23:58:24Z | 15020.67 EUR    | failed     |
| bithumb  | testuser1 | id5         | 2011-10-03T06:49:36Z | 69204866.9 DOGE | success    |
| huobi    | testuser2 | id2         | 2002-06-23T07:58:35Z | 37 XRP          | success    |
| huobi    | testuser1 | id1         | 1999-03-25T04:46:43Z | 0.045 BTC       | initial    |
| huobi    | testuser1 | id4         | 2011-10-03T06:49:36Z | 1.31 ETH        | processing |
+----------+-----------+-------------+----------------------+-----------------+------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterRecentDepositsNoConstraintsTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printRecentDeposits(DepositsPerExchange{}, constraints);
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
  basicQueryResultPrinter(ApiOutputType::json).printRecentDeposits(depositsPerExchange, constraints);
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
          "receivedTime": "2006-07-14T23:58:24Z",
          "status": "failed"
        },
        {
          "amount": "69204866.9",
          "cur": "DOGE",
          "id": "id5",
          "receivedTime": "2011-10-03T06:49:36Z",
          "status": "success"
        }
      ]
    },
    "huobi": {
      "testuser1": [
        {
          "amount": "0.045",
          "cur": "BTC",
          "id": "id1",
          "receivedTime": "1999-03-25T04:46:43Z",
          "status": "initial"
        },
        {
          "amount": "1.31",
          "cur": "ETH",
          "id": "id4",
          "receivedTime": "2011-10-03T06:49:36Z",
          "status": "processing"
        }
      ],
      "testuser2": [
        {
          "amount": "37",
          "cur": "XRP",
          "id": "id2",
          "receivedTime": "2002-06-23T07:58:35Z",
          "status": "success"
        }
      ]
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterRecentDepositsNoConstraintsTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::off).printRecentDeposits(depositsPerExchange, constraints);
  expectNoStr();
}

class QueryResultPrinterRecentWithdrawsBaseTest : public QueryResultPrinterTest {
 protected:
  Withdraw withdraw1{"id1", tp3, MonetaryAmount("0.045", "BTC"), Withdraw::Status::initial,
                     MonetaryAmount("0.00001", "BTC")};
  Withdraw withdraw2{"id2", tp4, MonetaryAmount(37, "XRP"), Withdraw::Status::success, MonetaryAmount("0.02", "XRP")};
  Withdraw withdraw3{"id3", tp1, MonetaryAmount("15020.67", "EUR"), Withdraw::Status::failed,
                     MonetaryAmount("0.1", "EUR")};
  Withdraw withdraw4{"id4", tp2, MonetaryAmount("1.31", "ETH"), Withdraw::Status::processing,
                     MonetaryAmount("0.001", "ETH")};
  Withdraw withdraw5{"id5", tp2, MonetaryAmount("69204866.9", "DOGE"), Withdraw::Status::success,
                     MonetaryAmount(2, "DOGE")};
};

class QueryResultPrinterRecentWithdrawsNoConstraintsTest : public QueryResultPrinterRecentWithdrawsBaseTest {
 protected:
  WithdrawsConstraints constraints;
  WithdrawsPerExchange withdrawsPerExchange{{&exchange1, WithdrawsSet{}},
                                            {&exchange2, WithdrawsSet{withdraw3, withdraw5}},
                                            {&exchange4, WithdrawsSet{withdraw2}},
                                            {&exchange3, WithdrawsSet{withdraw4, withdraw1}}};
};

TEST_F(QueryResultPrinterRecentWithdrawsNoConstraintsTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table).printRecentWithdraws(withdrawsPerExchange, constraints);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+-------------+----------------------+--------------------+-------------+------------+
| Exchange | Account   | Exchange Id | Sent time            | Net Emitted Amount | Fee         | Status     |
+----------+-----------+-------------+----------------------+--------------------+-------------+------------+
| bithumb  | testuser1 | id3         | 1999-03-25T04:46:43Z | 15020.67 EUR       | 0.1 EUR     | failed     |
| bithumb  | testuser1 | id5         | 2002-06-23T07:58:35Z | 69204866.9 DOGE    | 2 DOGE      | success    |
| huobi    | testuser2 | id2         | 2011-10-03T06:49:36Z | 37 XRP             | 0.02 XRP    | success    |
| huobi    | testuser1 | id4         | 2002-06-23T07:58:35Z | 1.31 ETH           | 0.001 ETH   | processing |
| huobi    | testuser1 | id1         | 2006-07-14T23:58:24Z | 0.045 BTC          | 0.00001 BTC | initial    |
+----------+-----------+-------------+----------------------+--------------------+-------------+------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterRecentWithdrawsNoConstraintsTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printRecentWithdraws(WithdrawsPerExchange{}, constraints);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "RecentWithdraws"
  },
  "out": {}
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterRecentWithdrawsNoConstraintsTest, Json) {
  basicQueryResultPrinter(ApiOutputType::json).printRecentWithdraws(withdrawsPerExchange, constraints);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "req": "RecentWithdraws"
  },
  "out": {
    "binance": {
      "testuser1": []
    },
    "bithumb": {
      "testuser1": [
        {
          "cur": "EUR",
          "fee": "0.1",
          "id": "id3",
          "netEmittedAmount": "15020.67",
          "sentTime": "1999-03-25T04:46:43Z",
          "status": "failed"
        },
        {
          "cur": "DOGE",
          "fee": "2",
          "id": "id5",
          "netEmittedAmount": "69204866.9",
          "sentTime": "2002-06-23T07:58:35Z",
          "status": "success"
        }
      ]
    },
    "huobi": {
      "testuser1": [
        {
          "cur": "ETH",
          "fee": "0.001",
          "id": "id4",
          "netEmittedAmount": "1.31",
          "sentTime": "2002-06-23T07:58:35Z",
          "status": "processing"
        },
        {
          "cur": "BTC",
          "fee": "0.00001",
          "id": "id1",
          "netEmittedAmount": "0.045",
          "sentTime": "2006-07-14T23:58:24Z",
          "status": "initial"
        }
      ],
      "testuser2": [
        {
          "cur": "XRP",
          "fee": "0.02",
          "id": "id2",
          "netEmittedAmount": "37",
          "sentTime": "2011-10-03T06:49:36Z",
          "status": "success"
        }
      ]
    }
  }
}
)";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterRecentWithdrawsNoConstraintsTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::off).printRecentWithdraws(withdrawsPerExchange, constraints);
  expectNoStr();
}

class QueryResultPrinterCancelOrdersTest : public QueryResultPrinterTest {
 protected:
  OrdersConstraints ordersConstraints;
  NbCancelledOrdersPerExchange nbCancelledOrdersPerExchange{
      {&exchange1, 2}, {&exchange2, 3}, {&exchange4, 1}, {&exchange3, 17}};
};

TEST_F(QueryResultPrinterCancelOrdersTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table).printCancelledOrders(nbCancelledOrdersPerExchange, ordersConstraints);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+----------------------------+
| Exchange | Account   | Number of cancelled orders |
+----------+-----------+----------------------------+
| binance  | testuser1 | 2                          |
| bithumb  | testuser1 | 3                          |
| huobi    | testuser2 | 1                          |
| huobi    | testuser1 | 17                         |
+----------+-----------+----------------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterCancelOrdersTest, EmptyJson) {
  basicQueryResultPrinter(ApiOutputType::json).printCancelledOrders(NbCancelledOrdersPerExchange{}, ordersConstraints);
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
  basicQueryResultPrinter(ApiOutputType::json).printCancelledOrders(nbCancelledOrdersPerExchange, ordersConstraints);
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
  basicQueryResultPrinter(ApiOutputType::off).printCancelledOrders(nbCancelledOrdersPerExchange, ordersConstraints);
  expectNoStr();
}

class QueryResultPrinterWithdrawTest : public QueryResultPrinterTest {
 protected:
  MonetaryAmount grossAmount{"76.55 XRP"};
  MonetaryAmount netEmittedAmount{"75.55 XRP"};
  MonetaryAmount fee = grossAmount - netEmittedAmount;
  ExchangeName fromExchange{exchange1.apiPrivate().exchangeName()};
  ExchangeName toExchange{exchange4.apiPrivate().exchangeName()};

  Wallet receivingWallet{toExchange,    grossAmount.currencyCode(),           "xrpaddress666", "xrptag2",
                         WalletCheck{}, AccountOwner("SmithJohn", "스미스존")};
  MonetaryAmount grossEmittedAmount;
  api::SentWithdrawInfo sentWithdrawInfo{netEmittedAmount, fee, Withdraw::Status::success};

  DeliveredWithdrawInfoWithExchanges deliveredWithdrawInfoWithExchanges{
      {&exchange1, &exchange4},
      DeliveredWithdrawInfo{api::InitiatedWithdrawInfo{receivingWallet, "WithdrawTest01", grossAmount, tp1},
                            api::ReceivedWithdrawInfo{"depositTest01", netEmittedAmount, tp2}}};
  WithdrawOptions withdrawOptions;
};

class QueryResultPrinterWithdrawAmountTest : public QueryResultPrinterWithdrawTest {
 protected:
  bool isPercentageWithdraw = false;
};

TEST_F(QueryResultPrinterWithdrawAmountTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table)
      .printWithdraw(deliveredWithdrawInfoWithExchanges, isPercentageWithdraw, withdrawOptions);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+-------------------------+-----------------------+--------------------------+
| Exchange | Account   | Sent -> Received amount | Sent -> Received time | Withdrawal -> Deposit id |
+----------+-----------+-------------------------+-----------------------+--------------------------+
| binance  | testuser1 | 76.55 XRP               | 1999-03-25T04:46:43Z  | WithdrawTest01           |
| huobi    | testuser2 | 75.55 XRP               | 2002-06-23T07:58:35Z  | depositTest01            |
+----------+-----------+-------------------------+-----------------------+--------------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterWithdrawAmountTest, Json) {
  basicQueryResultPrinter(ApiOutputType::json)
      .printWithdraw(deliveredWithdrawInfoWithExchanges, isPercentageWithdraw, withdrawOptions);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "XRP",
      "isPercentage": false,
      "syncPolicy": "synchronous"
    },
    "req": "Withdraw"
  },
  "out": {
    "from": {
      "account": "testuser1",
      "amount": "76.55",
      "exchange": "binance",
      "id": "WithdrawTest01",
      "time": "1999-03-25T04:46:43Z"
    },
    "to": {
      "account": "testuser2",
      "address": "xrpaddress666",
      "amount": "75.55",
      "exchange": "huobi",
      "id": "depositTest01",
      "tag": "xrptag2",
      "time": "2002-06-23T07:58:35Z"
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterWithdrawAmountTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::off)
      .printWithdraw(deliveredWithdrawInfoWithExchanges, isPercentageWithdraw, withdrawOptions);
  expectNoStr();
}

class QueryResultPrinterWithdrawPercentageTest : public QueryResultPrinterWithdrawTest {
 protected:
  bool isPercentageWithdraw = true;
};

TEST_F(QueryResultPrinterWithdrawPercentageTest, FormattedTable) {
  basicQueryResultPrinter(ApiOutputType::table)
      .printWithdraw(deliveredWithdrawInfoWithExchanges, isPercentageWithdraw, withdrawOptions);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+-------------------------+-----------------------+--------------------------+
| Exchange | Account   | Sent -> Received amount | Sent -> Received time | Withdrawal -> Deposit id |
+----------+-----------+-------------------------+-----------------------+--------------------------+
| binance  | testuser1 | 76.55 XRP               | 1999-03-25T04:46:43Z  | WithdrawTest01           |
| huobi    | testuser2 | 75.55 XRP               | 2002-06-23T07:58:35Z  | depositTest01            |
+----------+-----------+-------------------------+-----------------------+--------------------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterWithdrawPercentageTest, Json) {
  basicQueryResultPrinter(ApiOutputType::json)
      .printWithdraw(deliveredWithdrawInfoWithExchanges, isPercentageWithdraw, withdrawOptions);
  static constexpr std::string_view kExpected = R"(
{
  "in": {
    "opt": {
      "cur": "XRP",
      "isPercentage": true,
      "syncPolicy": "synchronous"
    },
    "req": "Withdraw"
  },
  "out": {
    "from": {
      "account": "testuser1",
      "amount": "76.55",
      "exchange": "binance",
      "id": "WithdrawTest01",
      "time": "1999-03-25T04:46:43Z"
    },
    "to": {
      "account": "testuser2",
      "address": "xrpaddress666",
      "amount": "75.55",
      "exchange": "huobi",
      "id": "depositTest01",
      "tag": "xrptag2",
      "time": "2002-06-23T07:58:35Z"
    }
  }
})";
  expectJson(kExpected);
}

TEST_F(QueryResultPrinterWithdrawPercentageTest, NoPrint) {
  basicQueryResultPrinter(ApiOutputType::off)
      .printWithdraw(deliveredWithdrawInfoWithExchanges, isPercentageWithdraw, withdrawOptions);
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
  basicQueryResultPrinter(ApiOutputType::table).printDustSweeper(tradedAmountsVectorWithFinalAmountPerExchange, cur);
  static constexpr std::string_view kExpected = R"(
+----------+-----------+-----------------------------+--------------+
| Exchange | Account   | Trades                      | Final Amount |
+----------+-----------+-----------------------------+--------------+
| binance  | testuser1 | 98.47 ETH -> 0.00005 BTC    | 0 ETH        |
| huobi    | testuser1 |                             | 1.56 ETH     |
|~~~~~~~~~~|~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~|
| huobi    | testuser2 | 0.45609 EUR -> 98.47 ETH    | 0 ETH        |
|          |           | 1509.45 ETH -> 0.000612 BTC |              |
+----------+-----------+-----------------------------+--------------+
)";
  expectStr(kExpected);
}

TEST_F(QueryResultPrinterDustSweeperTest, Json) {
  basicQueryResultPrinter(ApiOutputType::json).printDustSweeper(tradedAmountsVectorWithFinalAmountPerExchange, cur);
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
  basicQueryResultPrinter(ApiOutputType::off).printDustSweeper(tradedAmountsVectorWithFinalAmountPerExchange, cur);
  expectNoStr();
}

}  // namespace cct