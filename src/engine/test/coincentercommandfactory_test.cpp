#include "coincentercommandfactory.hpp"

#include <gtest/gtest.h>

#include <string_view>

#include "cct_const.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "coincentercommand.hpp"
#include "coincentercommandtype.hpp"
#include "coincenteroptions.hpp"
#include "currencycode.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "stringoptionparser.hpp"

namespace cct {

class CoincenterCommandFactoryTest : public ::testing::Test {
 protected:
  StringOptionParser &inputStr(std::string_view str) {
    optionParser = StringOptionParser(str);
    return optionParser;
  }

  StringOptionParser optionParser;
  CoincenterCmdLineOptions cmdLineOptions;
  const CoincenterCommand *pPreviousCommand{};
  CoincenterCommandFactory commandFactory{cmdLineOptions, pPreviousCommand};
};

TEST_F(CoincenterCommandFactoryTest, CreateMarketCommandEmpty) {
  EXPECT_EQ(CoincenterCommandFactory::CreateMarketCommand(inputStr("")),
            CoincenterCommand(CoincenterCommandType::Markets));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateMarketCommandInvalidWrongOrder) {
  CoincenterCommandFactory::CreateMarketCommand(inputStr("huobi_user2,eth"));
  EXPECT_THROW(optionParser.checkEndParsing(), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateMarketCommandInvalidWrongCurrencySeparator) {
  CoincenterCommandFactory::CreateMarketCommand(inputStr("eth,btc"));
  EXPECT_THROW(optionParser.checkEndParsing(), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateMarketCommandSingleExchange) {
  EXPECT_EQ(
      CoincenterCommandFactory::CreateMarketCommand(inputStr("huobi_user2")),
      CoincenterCommand(CoincenterCommandType::Markets).setExchangeNames(ExchangeNames({ExchangeName("huobi_user2")})));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateMarketCommandMarketTest) {
  EXPECT_EQ(CoincenterCommandFactory::CreateMarketCommand(inputStr("eth-usdt")),
            CoincenterCommand(CoincenterCommandType::Markets).setCur1("ETH").setCur2("USDT"));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateMarketCommandSingleCurTest) {
  EXPECT_EQ(
      CoincenterCommandFactory::CreateMarketCommand(inputStr("XLM,kraken,binance_user1")),
      CoincenterCommand(CoincenterCommandType::Markets)
          .setCur1("XLM")
          .setExchangeNames(ExchangeNames({ExchangeName("kraken"), ExchangeName(ExchangeNameEnum::binance, "user1")})));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateOrderCommandAll) {
  CoincenterCommandType type = CoincenterCommandType::OrdersOpened;
  EXPECT_EQ(commandFactory.createOrderCommand(type, inputStr("")),
            CoincenterCommand(type).setOrdersConstraints(OrdersConstraints{}));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateOrderCommandSingleCur) {
  CoincenterCommandType type = CoincenterCommandType::OrdersOpened;
  EXPECT_EQ(commandFactory.createOrderCommand(type, inputStr("AVAX")),
            CoincenterCommand(type).setOrdersConstraints(OrdersConstraints("AVAX")));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateOrderCommandMarketWithExchange) {
  CoincenterCommandType type = CoincenterCommandType::OrdersOpened;
  EXPECT_EQ(commandFactory.createOrderCommand(type, inputStr("AVAX-BTC,huobi")),
            CoincenterCommand(type)
                .setOrdersConstraints(OrdersConstraints("AVAX", "BTC"))
                .setExchangeNames(ExchangeNames({ExchangeName("huobi")})));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateTradeInvalidNegativeAmount) {
  CoincenterCommandType type = CoincenterCommandType::Trade;
  EXPECT_THROW(commandFactory.createTradeCommand(type, inputStr("-13XRP-BTC,binance_user2")), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateTradeInvalidSeveralTrades) {
  CoincenterCommandType type = CoincenterCommandType::Trade;
  cmdLineOptions.buy = "100%USDT";  // to set isSmartTrade to true, such that currency will not be parsed
  commandFactory.createTradeCommand(type, inputStr("13XRP-BTC,binance_user2"));

  EXPECT_THROW(optionParser.checkEndParsing(), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateTradeAbsolute) {
  CoincenterCommandType type = CoincenterCommandType::Trade;
  EXPECT_EQ(commandFactory.createTradeCommand(type, inputStr("13XRP-BTC,binance_user2")),
            CoincenterCommand(type)
                .setTradeOptions(cmdLineOptions.computeTradeOptions())
                .setAmount(MonetaryAmount("13XRP"))
                .setPercentageAmount(false)
                .setCur1("BTC")
                .setExchangeNames(ExchangeNames({ExchangeName(ExchangeNameEnum::binance, "user2")})));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateTradePercentage) {
  CoincenterCommandType type = CoincenterCommandType::Trade;
  EXPECT_EQ(
      commandFactory.createTradeCommand(type, inputStr("67.906%eth-USDT,huobi,upbit_user1")),
      CoincenterCommand(type)
          .setTradeOptions(cmdLineOptions.computeTradeOptions())
          .setAmount(MonetaryAmount("67.906ETH"))
          .setPercentageAmount(true)
          .setCur1("USDT")
          .setExchangeNames(ExchangeNames({ExchangeName("huobi"), ExchangeName(ExchangeNameEnum::upbit, "user1")})));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateBuyCommand) {
  CoincenterCommandType type = CoincenterCommandType::Buy;
  cmdLineOptions.buy = "whatever";
  EXPECT_EQ(commandFactory.createTradeCommand(type, inputStr("804XLM")),
            CoincenterCommand(type)
                .setTradeOptions(cmdLineOptions.computeTradeOptions())
                .setAmount(MonetaryAmount("804XLM")));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateSellCommand) {
  CoincenterCommandType type = CoincenterCommandType::Sell;
  cmdLineOptions.sell = "whatever";
  EXPECT_EQ(commandFactory.createTradeCommand(type, inputStr("0.76BTC,bithumb")),
            CoincenterCommand(type)
                .setTradeOptions(cmdLineOptions.computeTradeOptions())
                .setAmount(MonetaryAmount("0.76BTC"))
                .setExchangeNames(ExchangeNames({ExchangeName("bithumb")})));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateSellWithPreviousInvalidCommand) {
  CoincenterCommandType type = CoincenterCommandType::Sell;
  cmdLineOptions.sell = "whatever";
  EXPECT_THROW(commandFactory.createTradeCommand(type, inputStr("")), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateSellAllCommand) {
  CoincenterCommandType type = CoincenterCommandType::Sell;
  cmdLineOptions.sellAll = "whatever";
  EXPECT_EQ(commandFactory.createTradeCommand(type, inputStr("DOGE")),
            CoincenterCommand(type)
                .setTradeOptions(cmdLineOptions.computeTradeOptions())
                .setPercentageAmount(true)
                .setAmount(MonetaryAmount(100, "DOGE")));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateWithdrawInvalidNoPrevious) {
  EXPECT_THROW(commandFactory.createWithdrawApplyCommand(inputStr("")), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateWithdrawWithLessThan2Exchanges) {
  EXPECT_THROW(commandFactory.createWithdrawApplyCommand(inputStr("kraken")), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateWithdrawWithMoreThan2Exchanges) {
  EXPECT_THROW(commandFactory.createWithdrawApplyCommand(inputStr("bithumb-upbit_user3-kucoin")), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateWithdrawAbsoluteValid) {
  EXPECT_EQ(commandFactory.createWithdrawApplyCommand(inputStr("5000XRP,binance_user1-kucoin_user2")),
            CoincenterCommand(CoincenterCommandType::Withdraw)
                .setWithdrawOptions(cmdLineOptions.computeWithdrawOptions())
                .setAmount(MonetaryAmount("5000XRP"))
                .setExchangeNames(ExchangeNames({ExchangeName(ExchangeNameEnum::binance, "user1"),
                                                 ExchangeName(ExchangeNameEnum::kucoin, "user2")})));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateWithdrawPercentageValid) {
  EXPECT_EQ(commandFactory.createWithdrawApplyCommand(inputStr("43.25%ltc,bithumb-kraken")),
            CoincenterCommand(CoincenterCommandType::Withdraw)
                .setWithdrawOptions(cmdLineOptions.computeWithdrawOptions())
                .setAmount(MonetaryAmount("43.25LTC"))
                .setPercentageAmount(true)
                .setExchangeNames(ExchangeNames({ExchangeName("bithumb"), ExchangeName("kraken")})));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryTest, CreateWithdrawAllNoCurrencyInvalid) {
  EXPECT_THROW(commandFactory.createWithdrawApplyAllCommand(inputStr("binance_user2-kraken")), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateWithdrawAllLessThan2ExchangesInvalid) {
  EXPECT_THROW(commandFactory.createWithdrawApplyAllCommand(inputStr("bithumb_user4")), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateWithdrawAllMoreThan2ExchangesInvalid) {
  EXPECT_THROW(commandFactory.createWithdrawApplyAllCommand(inputStr("binance-kucoin-kraken-upbit")), invalid_argument);
}

TEST_F(CoincenterCommandFactoryTest, CreateWithdrawAllValid) {
  EXPECT_EQ(commandFactory.createWithdrawApplyAllCommand(inputStr("sol,upbit-kraken")),
            CoincenterCommand(CoincenterCommandType::Withdraw)
                .setWithdrawOptions(cmdLineOptions.computeWithdrawOptions())
                .setAmount(MonetaryAmount(100, "SOL"))
                .setPercentageAmount(true)
                .setExchangeNames(ExchangeNames({ExchangeName("upbit"), ExchangeName("kraken")})));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

class CoincenterCommandFactoryWithPreviousTest : public ::testing::Test {
 protected:
  StringOptionParser &inputStr(std::string_view str) {
    optionParser = StringOptionParser(str);
    return optionParser;
  }

  StringOptionParser optionParser;
  CoincenterCmdLineOptions cmdLineOptions;
  CoincenterCommand previousCommand{CoincenterCommandType::Trade};
  CoincenterCommandFactory commandFactory{cmdLineOptions, &previousCommand};
};

TEST_F(CoincenterCommandFactoryWithPreviousTest, CreateSellWithPreviousCommand) {
  CoincenterCommandType type = CoincenterCommandType::Sell;
  cmdLineOptions.sell = "whatever";
  EXPECT_EQ(commandFactory.createTradeCommand(type, inputStr("")),
            CoincenterCommand(type).setTradeOptions(cmdLineOptions.computeTradeOptions()));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

TEST_F(CoincenterCommandFactoryWithPreviousTest, CreateWithdrawInvalidNoExchange) {
  EXPECT_THROW(commandFactory.createWithdrawApplyCommand(inputStr("")), invalid_argument);
}

TEST_F(CoincenterCommandFactoryWithPreviousTest, CreateWithdrawInvalidMoreThan1Exchange) {
  EXPECT_THROW(commandFactory.createWithdrawApplyCommand(inputStr("kucoin-huobi")), invalid_argument);
}

TEST_F(CoincenterCommandFactoryWithPreviousTest, CreateWithdrawWithPreviousValid) {
  EXPECT_EQ(commandFactory.createWithdrawApplyCommand(inputStr("kraken_user1")),
            CoincenterCommand(CoincenterCommandType::Withdraw)
                .setWithdrawOptions(cmdLineOptions.computeWithdrawOptions())
                .setExchangeNames(ExchangeNames({ExchangeName(ExchangeNameEnum::kraken, "user1")})));
  EXPECT_NO_THROW(optionParser.checkEndParsing());
}

}  // namespace cct