#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <span>
#include <string_view>
#include <utility>

#include "accountowner.hpp"
#include "balanceoptions.hpp"
#include "cct_exception.hpp"
#include "currencycode.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchange.hpp"
#include "exchangedata_test.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangesorchestrator.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "ordersconstraints.hpp"
#include "queryresulttypes.hpp"
#include "requests-config.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawoptions.hpp"
#include "withdrawordeposit.hpp"

namespace cct {

using Type = CurrencyExchange::Type;

class ExchangeOrchestratorTest : public ExchangesBaseTest {
 protected:
  ExchangesOrchestrator exchangesOrchestrator{schema::RequestsConfig{}, std::span<Exchange>(&this->exchange1, 8)};
  BalanceOptions balanceOptions;
  WithdrawOptions withdrawOptions{Duration{}, WithdrawSyncPolicy::synchronous, WithdrawOptions::Mode::kReal};
};

TEST_F(ExchangeOrchestratorTest, BalanceNoEquivalentCurrencyUniqueExchange) {
  EXPECT_CALL(ExchangePrivate(exchange1), queryAccountBalance(balanceOptions))
      .WillOnce(testing::Return(balancePortfolio1));

  const ExchangeName privateExchangeNames[1] = {ExchangeName(exchange1.exchangeNameEnum(), exchange1.keyName())};
  BalancePerExchange ret{{&exchange1, balancePortfolio1}};
  EXPECT_EQ(exchangesOrchestrator.getBalance(privateExchangeNames, balanceOptions), ret);
}

TEST_F(ExchangeOrchestratorTest, BalanceNoEquivalentCurrencySeveralExchanges) {
  EXPECT_CALL(ExchangePrivate(exchange1), queryAccountBalance(balanceOptions))
      .WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(ExchangePrivate(exchange3), queryAccountBalance(balanceOptions))
      .WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(ExchangePrivate(exchange4), queryAccountBalance(balanceOptions))
      .WillOnce(testing::Return(balancePortfolio3));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.exchangeNameEnum(), exchange3.keyName()),
                                               ExchangeName(exchange1.exchangeNameEnum(), exchange1.keyName()),
                                               ExchangeName(exchange4.exchangeNameEnum(), exchange4.keyName())};
  BalancePerExchange ret{
      {&exchange1, balancePortfolio1}, {&exchange3, balancePortfolio2}, {&exchange4, balancePortfolio3}};
  EXPECT_EQ(exchangesOrchestrator.getBalance(privateExchangeNames, balanceOptions), ret);
}

TEST_F(ExchangeOrchestratorTest, DepositInfoUniqueExchanges) {
  CurrencyCode depositCurrency{"ETH"};

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange2.exchangeNameEnum(), exchange2.keyName())};

  CurrencyExchangeFlatSet tradableCurrencies2{
      CurrencyExchangeVector{CurrencyExchange(depositCurrency, CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto),
                             CurrencyExchange("XRP", CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(ExchangePrivate(exchange2), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  Wallet wallet2{privateExchangeNames[0],           depositCurrency, "address1", "", WalletCheck(),
                 AccountOwner("en_name", "ko_name")};
  EXPECT_CALL(ExchangePrivate(exchange2), queryDepositWallet(depositCurrency)).WillOnce(testing::Return(wallet2));

  WalletPerExchange ret{{&exchange2, wallet2}};
  EXPECT_EQ(exchangesOrchestrator.getDepositInfo(privateExchangeNames, depositCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, DepositInfoSeveralExchangesWithUnavailableDeposits) {
  CurrencyCode depositCurrency{"XRP"};

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.exchangeNameEnum(), exchange3.keyName()),
                                               ExchangeName(exchange1.exchangeNameEnum(), exchange1.keyName()),
                                               ExchangeName(exchange2.exchangeNameEnum(), exchange2.keyName()),
                                               ExchangeName(exchange4.exchangeNameEnum(), exchange4.keyName())};

  CurrencyExchangeFlatSet tradableCurrencies1{
      CurrencyExchangeVector{CurrencyExchange(depositCurrency, CurrencyExchange::Deposit::kUnavailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto),
                             CurrencyExchange("SHIB", CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(ExchangePrivate(exchange1), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));

  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{CurrencyExchange(
      "XLM", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(ExchangePrivate(exchange2), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  CurrencyExchangeFlatSet tradableCurrencies3{CurrencyExchangeVector{
      CurrencyExchange("BTC", CurrencyExchange::Deposit::kUnavailable, CurrencyExchange::Withdraw::kUnavailable,
                       Type::kCrypto),
      CurrencyExchange("SOL", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                       Type::kCrypto),
      CurrencyExchange(depositCurrency, CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kUnavailable,
                       Type::kCrypto),
      CurrencyExchange("EUR", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                       Type::kFiat),
  }};
  EXPECT_CALL(ExchangePrivate(exchange3), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies3));
  EXPECT_CALL(ExchangePrivate(exchange4), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies3));

  Wallet wallet31{privateExchangeNames[2],           depositCurrency, "address2", "tag2", WalletCheck(),
                  AccountOwner("en_name", "ko_name")};
  EXPECT_CALL(ExchangePrivate(exchange3), queryDepositWallet(depositCurrency)).WillOnce(testing::Return(wallet31));

  Wallet wallet32{privateExchangeNames[3],           depositCurrency, "address3", "tag3", WalletCheck(),
                  AccountOwner("en_name", "ko_name")};
  EXPECT_CALL(ExchangePrivate(exchange4), queryDepositWallet(depositCurrency)).WillOnce(testing::Return(wallet32));

  WalletPerExchange ret{{&exchange3, wallet31}, {&exchange4, wallet32}};
  EXPECT_EQ(exchangesOrchestrator.getDepositInfo(privateExchangeNames, depositCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, GetOpenedOrders) {
  OrdersConstraints noConstraints;

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.exchangeNameEnum(), exchange3.keyName()),
                                               ExchangeName(exchange2.exchangeNameEnum(), exchange2.keyName()),
                                               ExchangeName(exchange4.exchangeNameEnum(), exchange4.keyName())};

  OpenedOrderVector openedOrders2{OpenedOrder("Id1", MonetaryAmount("0.1ETH"), MonetaryAmount("0.9ETH"),
                                              MonetaryAmount("0.14BTC"), Clock::now(), TradeSide::buy),
                                  OpenedOrder("Id2", MonetaryAmount("15XLM"), MonetaryAmount("76XLM"),
                                              MonetaryAmount("0.5EUR"), Clock::now(), TradeSide::sell)};
  EXPECT_CALL(ExchangePrivate(exchange2), queryOpenedOrders(noConstraints)).WillOnce(testing::Return(openedOrders2));

  OpenedOrderVector openedOrders3{};
  EXPECT_CALL(ExchangePrivate(exchange3), queryOpenedOrders(noConstraints)).WillOnce(testing::Return(openedOrders3));

  OpenedOrderVector openedOrders4{OpenedOrder("Id37", MonetaryAmount("0.7ETH"), MonetaryAmount("0.9ETH"),
                                              MonetaryAmount("0.14BTC"), Clock::now(), TradeSide::sell),
                                  OpenedOrder("Id2", MonetaryAmount("15XLM"), MonetaryAmount("19XLM"),
                                              MonetaryAmount("0.5EUR"), Clock::now(), TradeSide::buy)};
  EXPECT_CALL(ExchangePrivate(exchange4), queryOpenedOrders(noConstraints)).WillOnce(testing::Return(openedOrders4));

  OpenedOrdersPerExchange ret{{&exchange2, OpenedOrderSet(openedOrders2.begin(), openedOrders2.end())},
                              {&exchange3, OpenedOrderSet(openedOrders3.begin(), openedOrders3.end())},
                              {&exchange4, OpenedOrderSet(openedOrders4.begin(), openedOrders4.end())}};
  EXPECT_EQ(exchangesOrchestrator.getOpenedOrders(privateExchangeNames, noConstraints), ret);
}

TEST_F(ExchangeOrchestratorTest, WithdrawSameAccountImpossible) {
  MonetaryAmount grossAmount{1000, "XRP"};
  ExchangeName fromExchange(exchange1.exchangeNameEnum(), exchange1.keyName());
  const ExchangeName &toExchange = fromExchange;
  EXPECT_THROW(exchangesOrchestrator.withdraw(grossAmount, false, fromExchange, toExchange, withdrawOptions),
               exception);
}

TEST_F(ExchangeOrchestratorTest, WithdrawImpossibleFrom) {
  MonetaryAmount grossAmount{1000, "XRP"};
  ExchangeName fromExchange(exchange1.exchangeNameEnum(), exchange1.keyName());
  ExchangeName toExchange(exchange2.exchangeNameEnum(), exchange2.keyName());

  CurrencyExchangeFlatSet tradableCurrencies1{
      CurrencyExchangeVector{CurrencyExchange(grossAmount.currencyCode(), CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kUnavailable, Type::kCrypto),
                             CurrencyExchange("SHIB", CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(ExchangePrivate(exchange1), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
  CurrencyExchangeFlatSet tradableCurrencies2{
      CurrencyExchangeVector{CurrencyExchange(grossAmount.currencyCode(), CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto),
                             CurrencyExchange("SHIB", CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(ExchangePrivate(exchange2), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  auto [exchanges, deliveredWithdrawInfo] =
      exchangesOrchestrator.withdraw(grossAmount, false, fromExchange, toExchange, withdrawOptions);
  EXPECT_FALSE(deliveredWithdrawInfo.hasBeenInitiated());
}

TEST_F(ExchangeOrchestratorTest, WithdrawImpossibleTo) {
  MonetaryAmount grossAmount{1000, "XRP"};
  ExchangeName fromExchange(exchange1.exchangeNameEnum(), exchange1.keyName());
  ExchangeName toExchange(exchange2.exchangeNameEnum(), exchange2.keyName());

  CurrencyExchangeFlatSet tradableCurrencies1{
      CurrencyExchangeVector{CurrencyExchange(grossAmount.currencyCode(), CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto),
                             CurrencyExchange("SHIB", CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(ExchangePrivate(exchange1), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
  CurrencyExchangeFlatSet tradableCurrencies2{
      CurrencyExchangeVector{CurrencyExchange(grossAmount.currencyCode(), CurrencyExchange::Deposit::kUnavailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto),
                             CurrencyExchange("SHIB", CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(ExchangePrivate(exchange2), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  const auto [exchanges, deliveredWithdrawInfo] =
      exchangesOrchestrator.withdraw(grossAmount, false, fromExchange, toExchange, withdrawOptions);
  EXPECT_FALSE(deliveredWithdrawInfo.hasBeenInitiated());
}

inline bool operator==(const DeliveredWithdrawInfo &lhs, const DeliveredWithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}

namespace api {
inline bool operator==(const InitiatedWithdrawInfo &lhs, const InitiatedWithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}

inline bool operator==(const SentWithdrawInfo &lhs, const SentWithdrawInfo &rhs) {
  return lhs.withdrawStatus() == rhs.withdrawStatus() && lhs.netEmittedAmount() == rhs.netEmittedAmount();
}
}  // namespace api

class ExchangeOrchestratorWithdrawTest : public ExchangeOrchestratorTest {
 protected:
  ExchangeOrchestratorWithdrawTest() {
    CurrencyExchangeFlatSet tradableCurrencies1{
        CurrencyExchangeVector{CurrencyExchange(cur, CurrencyExchange::Deposit::kUnavailable,
                                                CurrencyExchange::Withdraw::kAvailable, Type::kCrypto),
                               CurrencyExchange("SHIB", CurrencyExchange::Deposit::kAvailable,
                                                CurrencyExchange::Withdraw::kAvailable, Type::kCrypto)}};
    CurrencyExchangeFlatSet tradableCurrencies2{
        CurrencyExchangeVector{CurrencyExchange(cur, CurrencyExchange::Deposit::kAvailable,
                                                CurrencyExchange::Withdraw::kUnavailable, Type::kCrypto),
                               CurrencyExchange("SHIB", CurrencyExchange::Deposit::kAvailable,
                                                CurrencyExchange::Withdraw::kAvailable, Type::kCrypto)}};

    EXPECT_CALL(ExchangePrivate(exchange1), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
    EXPECT_CALL(ExchangePrivate(exchange2), queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));
  }

  DeliveredWithdrawInfo createWithdrawInfo(MonetaryAmount grossAmount, bool isPercentageWithdraw) {
    if (isPercentageWithdraw) {
      EXPECT_CALL(ExchangePrivate(exchange1), queryAccountBalance(balanceOptions))
          .WillOnce(testing::Return(balancePortfolio1));
      grossAmount = (grossAmount.toNeutral() * balancePortfolio1.get(cur)) / 100;
    } else {
      EXPECT_CALL(ExchangePrivate(exchange1), queryAccountBalance(testing::_)).Times(0);
    }
    MonetaryAmount netEmittedAmount = grossAmount - fee;
    Wallet receivingWallet{toExchange, cur,           "TestAddress",
                           "TestTag",  WalletCheck(), AccountOwner("SmithJohn", "스미스존")};
    EXPECT_CALL(ExchangePrivate(exchange2), queryDepositWallet(cur)).WillOnce(testing::Return(receivingWallet));

    api::InitiatedWithdrawInfo initiatedWithdrawInfo{receivingWallet, withdrawId, grossAmount};
    EXPECT_CALL(ExchangePrivate(exchange1), launchWithdraw(grossAmount, std::move(receivingWallet)))
        .WillOnce(testing::Return(initiatedWithdrawInfo));

    api::SentWithdrawInfo sentWithdrawInfo{netEmittedAmount, fee, Withdraw::Status::success};
    EXPECT_CALL(ExchangePrivate(exchange1), queryRecentWithdraws(testing::_))
        .WillOnce(testing::Return(
            WithdrawsSet{Withdraw{withdrawId, withdrawTimestamp, netEmittedAmount, Withdraw::Status::success, fee}}));

    api::ReceivedWithdrawInfo receivedWithdrawInfo{"deposit-id", netEmittedAmount};
    EXPECT_CALL(ExchangePrivate(exchange2), queryWithdrawDelivery(initiatedWithdrawInfo, sentWithdrawInfo))
        .WillOnce(testing::Return(receivedWithdrawInfo));

    return {std::move(initiatedWithdrawInfo), std::move(receivedWithdrawInfo)};
  }

  CurrencyCode cur{"XRP"};
  ExchangeName fromExchange{exchange1.exchangeNameEnum(), exchange1.keyName()};
  ExchangeName toExchange{exchange2.exchangeNameEnum(), exchange2.keyName()};

  MonetaryAmount fee{"0.02", cur};
  std::string_view withdrawId = "WithdrawId";
  TimePoint withdrawTimestamp = Clock::now();
};

TEST_F(ExchangeOrchestratorWithdrawTest, WithdrawPossible) {
  MonetaryAmount grossAmount{1000, cur};
  bool isPercentageWithdraw = false;
  auto exp = createWithdrawInfo(grossAmount, isPercentageWithdraw);
  auto [exchanges, ret] =
      exchangesOrchestrator.withdraw(grossAmount, isPercentageWithdraw, fromExchange, toExchange, withdrawOptions);
  EXPECT_EQ(exp, ret);
}

TEST_F(ExchangeOrchestratorWithdrawTest, WithdrawPossiblePercentage) {
  MonetaryAmount grossAmount{25, cur};
  bool isPercentageWithdraw = true;
  auto exp = createWithdrawInfo(grossAmount, isPercentageWithdraw);
  auto [exchanges, ret] =
      exchangesOrchestrator.withdraw(grossAmount, isPercentageWithdraw, fromExchange, toExchange, withdrawOptions);
  EXPECT_EQ(exp, ret);
}
}  // namespace cct
