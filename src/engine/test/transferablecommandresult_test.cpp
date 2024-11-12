#include "transferablecommandresult.hpp"

#include <gtest/gtest.h>

#include <utility>

#include "cct_exception.hpp"
#include "coincentercommand.hpp"
#include "coincentercommandtype.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "monetaryamount.hpp"

namespace cct {

class TransferableCommandResultTest : public ::testing::Test {
 protected:
  static CoincenterCommand createCommand(CoincenterCommandType type, MonetaryAmount amt = MonetaryAmount(),
                                         bool isPercentage = false, ExchangeNames exchangeNames = ExchangeNames()) {
    CoincenterCommand cmd{type};
    cmd.setAmount(amt);
    cmd.setPercentageAmount(isPercentage);
    cmd.setExchangeNames(std::move(exchangeNames));
    return cmd;
  }

  ExchangeName exchangeName11{ExchangeNameEnum::binance, "user1"};
  ExchangeName exchangeName12{ExchangeNameEnum::binance, "user2"};

  ExchangeName exchangeName21{ExchangeNameEnum::kraken, "user1"};
  ExchangeName exchangeName22{ExchangeNameEnum::kraken, "user2"};

  MonetaryAmount amount11{50, "DOGE"};
  MonetaryAmount amount12{10, "DOGE"};
  MonetaryAmount amount13{5, "DOGE"};

  MonetaryAmount amount21{"0.56BTC"};
  MonetaryAmount amount22{"0.14BTC"};
};

TEST_F(TransferableCommandResultTest, ComputeTradeAmountAndExchangesUniqueAmount) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11}};
  EXPECT_EQ(ComputeTradeAmountAndExchanges(createCommand(CoincenterCommandType::Trade), previousResults),
            std::make_pair(MonetaryAmount{50, "DOGE"}, ExchangeNames({exchangeName11})));
}

TEST_F(TransferableCommandResultTest, ComputeTradeAmountAndExchangesDoubleAmountsSameExchange) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11},
                                                       TransferableCommandResult{exchangeName11, amount12}};
  EXPECT_EQ(ComputeTradeAmountAndExchanges(createCommand(CoincenterCommandType::Trade), previousResults),
            std::make_pair(MonetaryAmount{60, "DOGE"}, ExchangeNames({exchangeName11})));
}

TEST_F(TransferableCommandResultTest, ComputeTradeAmountAndExchangesDoubleAmountsDifferentExchanges) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11},
                                                       TransferableCommandResult{exchangeName22, amount12}};
  EXPECT_EQ(ComputeTradeAmountAndExchanges(createCommand(CoincenterCommandType::Trade), previousResults),
            std::make_pair(MonetaryAmount{60, "DOGE"}, ExchangeNames({exchangeName11, exchangeName22})));
}

TEST_F(TransferableCommandResultTest, ComputeTradeAmountAndExchangesTripleAmounts) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11},
                                                       TransferableCommandResult{exchangeName11, amount12},
                                                       TransferableCommandResult{exchangeName21, amount13}};
  EXPECT_EQ(ComputeTradeAmountAndExchanges(createCommand(CoincenterCommandType::Trade), previousResults),
            std::make_pair(MonetaryAmount{65, "DOGE"}, ExchangeNames({exchangeName11, exchangeName21})));
}

TEST_F(TransferableCommandResultTest, ComputeTradeAmountAndExchangesDoubleAmountsInvalid) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11},
                                                       TransferableCommandResult{exchangeName22, amount21}};
  EXPECT_EQ(ComputeTradeAmountAndExchanges(createCommand(CoincenterCommandType::Trade), previousResults),
            std::make_pair(MonetaryAmount(), ExchangeNames()));
}

TEST_F(TransferableCommandResultTest, ComputeTradeAmountAndExchangesWithFullInformation) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11},
                                                       TransferableCommandResult{exchangeName22, amount21}};
  EXPECT_EQ(ComputeTradeAmountAndExchanges(createCommand(CoincenterCommandType::Trade, MonetaryAmount(100, "DOGE")),
                                           previousResults),
            std::make_pair(MonetaryAmount(100, "DOGE"), ExchangeNames()));
}

TEST_F(TransferableCommandResultTest, ComputeTradeAmountAndExchangesUnexpectedSituation) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11},
                                                       TransferableCommandResult{exchangeName22, amount21}};

  EXPECT_THROW(ComputeTradeAmountAndExchanges(createCommand(CoincenterCommandType::Trade, MonetaryAmount(), true),
                                              previousResults),
               exception);
  EXPECT_THROW(ComputeTradeAmountAndExchanges(createCommand(CoincenterCommandType::Trade, MonetaryAmount(), false,
                                                            ExchangeNames({exchangeName11})),
                                              previousResults),
               exception);
}

TEST_F(TransferableCommandResultTest, ComputeWithdrawAmountInvalidSingleExchangeAmount) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11}};

  EXPECT_THROW(ComputeWithdrawAmount(
                   createCommand(CoincenterCommandType::Trade, amount12, false, ExchangeNames({exchangeName11})),
                   previousResults),
               exception);
}

TEST_F(TransferableCommandResultTest, ComputeWithdrawAmountValidSingleExchange) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11}};

  EXPECT_EQ(ComputeWithdrawAmount(
                createCommand(CoincenterCommandType::Trade, MonetaryAmount(), false, ExchangeNames({exchangeName12})),
                previousResults),
            std::make_pair(amount11, exchangeName11));
}

TEST_F(TransferableCommandResultTest, ComputeWithdrawAmountInvalidSingleExchangeTooManyTransferableResults) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11},
                                                       TransferableCommandResult{exchangeName21, amount12}};

  EXPECT_EQ(ComputeWithdrawAmount(
                createCommand(CoincenterCommandType::Trade, MonetaryAmount(), false, ExchangeNames({exchangeName12})),
                previousResults),
            std::make_pair(MonetaryAmount(), ExchangeName()));
}

TEST_F(TransferableCommandResultTest, ComputeWithdrawAmountInvalidTooManyExchanges) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11}};

  EXPECT_THROW(ComputeWithdrawAmount(createCommand(CoincenterCommandType::Trade, amount12, false,
                                                   ExchangeNames({exchangeName11, exchangeName12, exchangeName22})),
                                     previousResults),
               exception);
}

TEST_F(TransferableCommandResultTest, ComputeWithdrawAmountInvalidNoExchange) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11}};

  EXPECT_THROW(ComputeWithdrawAmount(createCommand(CoincenterCommandType::Trade), previousResults), exception);
}

TEST_F(TransferableCommandResultTest, ComputeWithdrawAmountValidDoubleExchange) {
  const TransferableCommandResult previousResults[] = {TransferableCommandResult{exchangeName11, amount11}};

  EXPECT_EQ(ComputeWithdrawAmount(createCommand(CoincenterCommandType::Trade, amount22, false,
                                                ExchangeNames({exchangeName12, exchangeName21})),
                                  previousResults),
            std::make_pair(amount22, exchangeName12));
}
}  // namespace cct