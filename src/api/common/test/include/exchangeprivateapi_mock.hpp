#pragma once

#include <gmock/gmock.h>

#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "tradeinfo.hpp"
#include "wallet.hpp"
#include "withdrawinfo.hpp"
#include "withdrawsconstraints.hpp"

namespace cct::api {

class MockExchangePrivate : public ExchangePrivate {
 public:
  MockExchangePrivate(ExchangePublic &exchangePublic, const CoincenterInfo &config, const APIKey &apiKey)
      : ExchangePrivate(config, exchangePublic, apiKey) {}

  MOCK_METHOD(bool, validateApiKey, (), (override));
  MOCK_METHOD(CurrencyExchangeFlatSet, queryTradableCurrencies, (), (override));
  MOCK_METHOD(BalancePortfolio, queryAccountBalance, (const BalanceOptions &), (override));
  MOCK_METHOD(Wallet, queryDepositWallet, (CurrencyCode), (override));
  MOCK_METHOD(bool, canGenerateDepositAddress, (), (const override));
  MOCK_METHOD(ClosedOrderVector, queryClosedOrders, (const OrdersConstraints &), (override));
  MOCK_METHOD(OpenedOrderVector, queryOpenedOrders, (const OrdersConstraints &), (override));
  MOCK_METHOD(int, cancelOpenedOrders, (const OrdersConstraints &), (override));

  MOCK_METHOD(DepositsSet, queryRecentDeposits, (const DepositsConstraints &), (override));

  MOCK_METHOD(WithdrawsSet, queryRecentWithdraws, (const WithdrawsConstraints &), (override));

  MOCK_METHOD(bool, isSimulatedOrderSupported, (), (const override));

  MOCK_METHOD(PlaceOrderInfo, placeOrder, (MonetaryAmount, MonetaryAmount, MonetaryAmount, const TradeInfo &),
              (override));
  MOCK_METHOD(OrderInfo, cancelOrder, (OrderIdView, const TradeContext &), (override));
  MOCK_METHOD(OrderInfo, queryOrderInfo, (OrderIdView, const TradeContext &), (override));
  MOCK_METHOD(InitiatedWithdrawInfo, launchWithdraw, (MonetaryAmount, Wallet &&), (override));
  MOCK_METHOD(ReceivedWithdrawInfo, queryWithdrawDelivery, (const InitiatedWithdrawInfo &, const SentWithdrawInfo &),
              (override));
};

}  // namespace cct::api