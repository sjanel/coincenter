#include <gtest/gtest.h>

#include "cct_const.hpp"
#include "exchangedata_test.hpp"
#include "exchangesorchestrator.hpp"

namespace cct {

using Deposit = CurrencyExchange::Deposit;
using Withdraw = CurrencyExchange::Withdraw;
using Type = CurrencyExchange::Type;

class ExchangeOrchestratorTest : public ExchangesBaseTest {
 protected:
  ExchangesOrchestrator exchangesOrchestrator{std::span<Exchange>(&this->exchange1, 8)};
};

TEST_F(ExchangeOrchestratorTest, BalanceNoEquivalentCurrencyUniqueExchange) {
  CurrencyCode equiCurrency;

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio1));

  const ExchangeName privateExchangeNames[1] = {ExchangeName(exchange1.name(), exchange1.keyName())};
  BalancePerExchange ret{{&exchange1, balancePortfolio1}};
  EXPECT_EQ(exchangesOrchestrator.getBalance(privateExchangeNames, equiCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, BalanceNoEquivalentCurrencySeveralExchanges) {
  CurrencyCode equiCurrency;

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio3));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.name(), exchange3.keyName()),
                                               ExchangeName(exchange1.name(), exchange1.keyName()),
                                               ExchangeName(exchange4.name(), exchange4.keyName())};
  BalancePerExchange ret{
      {&exchange1, balancePortfolio1}, {&exchange3, balancePortfolio2}, {&exchange4, balancePortfolio3}};
  EXPECT_EQ(exchangesOrchestrator.getBalance(privateExchangeNames, equiCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, DepositInfoUniqueExchanges) {
  CurrencyCode depositCurrency{"ETH"};

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange2.name(), exchange2.keyName())};

  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange(depositCurrency, Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("XRP", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  Wallet wallet2{privateExchangeNames[0], depositCurrency, "address1", "", WalletCheck()};
  EXPECT_CALL(exchangePrivate2, queryDepositWallet(depositCurrency)).WillOnce(testing::Return(wallet2));

  WalletPerExchange ret{{&exchange2, wallet2}};
  EXPECT_EQ(exchangesOrchestrator.getDepositInfo(privateExchangeNames, depositCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, DepositInfoSeveralExchangesWithUnavailableDeposits) {
  CurrencyCode depositCurrency{"XRP"};

  const ExchangeName privateExchangeNames[] = {
      ExchangeName(exchange3.name(), exchange3.keyName()), ExchangeName(exchange1.name(), exchange1.keyName()),
      ExchangeName(exchange2.name(), exchange2.keyName()), ExchangeName(exchange4.name(), exchange4.keyName())};

  CurrencyExchangeFlatSet tradableCurrencies1{CurrencyExchangeVector{
      CurrencyExchange(depositCurrency, Deposit::kUnavailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));

  CurrencyExchangeFlatSet tradableCurrencies2{
      CurrencyExchangeVector{CurrencyExchange("XLM", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  CurrencyExchangeFlatSet tradableCurrencies3{CurrencyExchangeVector{
      CurrencyExchange("BTC", Deposit::kUnavailable, Withdraw::kUnavailable, Type::kCrypto),
      CurrencyExchange("SOL", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange(depositCurrency, Deposit::kAvailable, Withdraw::kUnavailable, Type::kCrypto),
      CurrencyExchange("EUR", Deposit::kAvailable, Withdraw::kAvailable, Type::kFiat),
  }};
  EXPECT_CALL(exchangePrivate3, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies3));
  EXPECT_CALL(exchangePrivate4, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies3));

  Wallet wallet31{privateExchangeNames[2], depositCurrency, "address2", "tag2", WalletCheck()};
  EXPECT_CALL(exchangePrivate3, queryDepositWallet(depositCurrency)).WillOnce(testing::Return(wallet31));

  Wallet wallet32{privateExchangeNames[3], depositCurrency, "address3", "tag3", WalletCheck()};
  EXPECT_CALL(exchangePrivate4, queryDepositWallet(depositCurrency)).WillOnce(testing::Return(wallet32));

  WalletPerExchange ret{{&exchange3, wallet31}, {&exchange4, wallet32}};
  EXPECT_EQ(exchangesOrchestrator.getDepositInfo(privateExchangeNames, depositCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, GetOpenedOrders) {
  OrdersConstraints noConstraints;

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.name(), exchange3.keyName()),
                                               ExchangeName(exchange2.name(), exchange2.keyName()),
                                               ExchangeName(exchange4.name(), exchange4.keyName())};

  Orders orders2{Order("Id1", MonetaryAmount("0.1ETH"), MonetaryAmount("0.9ETH"), MonetaryAmount("0.14BTC"),
                       Clock::now(), TradeSide::kBuy),
                 Order("Id2", MonetaryAmount("15XLM"), MonetaryAmount("76XLM"), MonetaryAmount("0.5EUR"), Clock::now(),
                       TradeSide::kSell)};
  EXPECT_CALL(exchangePrivate2, queryOpenedOrders(noConstraints)).WillOnce(testing::Return(orders2));

  Orders orders3{};
  EXPECT_CALL(exchangePrivate3, queryOpenedOrders(noConstraints)).WillOnce(testing::Return(orders3));

  Orders orders4{Order("Id37", MonetaryAmount("0.7ETH"), MonetaryAmount("0.9ETH"), MonetaryAmount("0.14BTC"),
                       Clock::now(), TradeSide::kSell),
                 Order("Id2", MonetaryAmount("15XLM"), MonetaryAmount("19XLM"), MonetaryAmount("0.5EUR"), Clock::now(),
                       TradeSide::kBuy)};
  EXPECT_CALL(exchangePrivate4, queryOpenedOrders(noConstraints)).WillOnce(testing::Return(orders4));

  OpenedOrdersPerExchange ret{{&exchange2, OrdersSet(orders2.begin(), orders2.end())},
                              {&exchange3, OrdersSet(orders3.begin(), orders3.end())},
                              {&exchange4, OrdersSet(orders4.begin(), orders4.end())}};
  EXPECT_EQ(exchangesOrchestrator.getOpenedOrders(privateExchangeNames, noConstraints), ret);
}

TEST_F(ExchangeOrchestratorTest, WithdrawSameAccountImpossible) {
  MonetaryAmount grossAmount{1000, "XRP"};
  ExchangeName fromExchange(exchange1.name(), exchange1.keyName());
  const ExchangeName &toExchange = fromExchange;
  EXPECT_THROW(exchangesOrchestrator.withdraw(grossAmount, false, fromExchange, toExchange), exception);
}

TEST_F(ExchangeOrchestratorTest, WithdrawImpossibleFrom) {
  MonetaryAmount grossAmount{1000, "XRP"};
  ExchangeName fromExchange(exchange1.name(), exchange1.keyName());
  ExchangeName toExchange(exchange2.name(), exchange2.keyName());

  CurrencyExchangeFlatSet tradableCurrencies1{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kAvailable, Withdraw::kUnavailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  EXPECT_FALSE(exchangesOrchestrator.withdraw(grossAmount, false, fromExchange, toExchange).hasBeenInitiated());
}

TEST_F(ExchangeOrchestratorTest, WithdrawImpossibleTo) {
  MonetaryAmount grossAmount{1000, "XRP"};
  ExchangeName fromExchange(exchange1.name(), exchange1.keyName());
  ExchangeName toExchange(exchange2.name(), exchange2.keyName());

  CurrencyExchangeFlatSet tradableCurrencies1{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kUnavailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  EXPECT_FALSE(exchangesOrchestrator.withdraw(grossAmount, false, fromExchange, toExchange).hasBeenInitiated());
}

inline bool operator==(const WithdrawInfo &lhs, const WithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}

namespace api {
inline bool operator==(const InitiatedWithdrawInfo &lhs, const InitiatedWithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}

inline bool operator==(const SentWithdrawInfo &lhs, const SentWithdrawInfo &rhs) {
  return lhs.isWithdrawSent() == rhs.isWithdrawSent() && lhs.netEmittedAmount() == rhs.netEmittedAmount();
}
}  // namespace api

class ExchangeOrchestratorWithdrawTest : public ExchangeOrchestratorTest {
 protected:
  ExchangeOrchestratorWithdrawTest() {
    CurrencyExchangeFlatSet tradableCurrencies1{
        CurrencyExchangeVector{CurrencyExchange(cur, Deposit::kUnavailable, Withdraw::kAvailable, Type::kCrypto),
                               CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
    CurrencyExchangeFlatSet tradableCurrencies2{
        CurrencyExchangeVector{CurrencyExchange(cur, Deposit::kAvailable, Withdraw::kUnavailable, Type::kCrypto),
                               CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};

    EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
    EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));
  }

  WithdrawInfo createWithdrawInfo(MonetaryAmount grossAmount, bool isPercentageWithdraw) {
    if (isPercentageWithdraw) {
      EXPECT_CALL(exchangePrivate1, queryAccountBalance(CurrencyCode())).WillOnce(testing::Return(balancePortfolio1));
      grossAmount = (grossAmount.toNeutral() * balancePortfolio1.get(cur)) / 100;
    } else {
      EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).Times(0);
    }
    MonetaryAmount netEmittedAmount = grossAmount - fee;
    Wallet receivingWallet{toExchange, cur, address, tag, WalletCheck()};
    EXPECT_CALL(exchangePrivate2, queryDepositWallet(cur)).WillOnce(testing::Return(receivingWallet));

    api::InitiatedWithdrawInfo initiatedWithdrawInfo{receivingWallet, withdrawIdView, grossAmount};
    EXPECT_CALL(exchangePrivate1, launchWithdraw(grossAmount, std::move(receivingWallet)))
        .WillOnce(testing::Return(initiatedWithdrawInfo));
    api::SentWithdrawInfo sentWithdrawInfo{netEmittedAmount, true};
    EXPECT_CALL(exchangePrivate1, isWithdrawSuccessfullySent(initiatedWithdrawInfo))
        .WillOnce(testing::Return(sentWithdrawInfo));
    EXPECT_CALL(exchangePrivate2, isWithdrawReceived(initiatedWithdrawInfo, sentWithdrawInfo))
        .WillOnce(testing::Return(true));
    return WithdrawInfo(initiatedWithdrawInfo, sentWithdrawInfo);
  }

  CurrencyCode cur{"XRP"};
  ExchangeName fromExchange{exchange1.name(), exchange1.keyName()};
  ExchangeName toExchange{exchange2.name(), exchange2.keyName()};

  std::string_view address{"TestAddress"};
  std::string_view tag{"TestTag"};

  WithdrawIdView withdrawIdView{"WithdrawId"};

  MonetaryAmount fee{"0.02", cur};
};

TEST_F(ExchangeOrchestratorWithdrawTest, WithdrawPossible) {
  MonetaryAmount grossAmount{1000, cur};
  bool isPercentageWithdraw = false;
  auto exp = createWithdrawInfo(grossAmount, isPercentageWithdraw);
  auto ret =
      exchangesOrchestrator.withdraw(grossAmount, isPercentageWithdraw, fromExchange, toExchange, Duration::zero());
  EXPECT_EQ(exp, ret);
}

TEST_F(ExchangeOrchestratorWithdrawTest, WithdrawPossiblePercentage) {
  MonetaryAmount grossAmount{25, cur};
  bool isPercentageWithdraw = true;
  auto exp = createWithdrawInfo(grossAmount, isPercentageWithdraw);
  auto ret =
      exchangesOrchestrator.withdraw(grossAmount, isPercentageWithdraw, fromExchange, toExchange, Duration::zero());
  EXPECT_EQ(exp, ret);
}
}  // namespace cct