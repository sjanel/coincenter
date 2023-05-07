#pragma once

#include <gmock/gmock.h>

#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"

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
  MOCK_METHOD(Orders, queryOpenedOrders, (const OrdersConstraints &), (override));
  MOCK_METHOD(int, cancelOpenedOrders, (const OrdersConstraints &), (override));

  MOCK_METHOD(DepositsSet, queryRecentDeposits, (const DepositsConstraints &), (override));

  MOCK_METHOD(WithdrawsSet, queryRecentWithdraws, (const WithdrawsConstraints &), (override));

  MOCK_METHOD(bool, isSimulatedOrderSupported, (), (const override));

  MOCK_METHOD(PlaceOrderInfo, placeOrder, (MonetaryAmount, MonetaryAmount, MonetaryAmount, const TradeInfo &),
              (override));
  MOCK_METHOD(OrderInfo, cancelOrder, (OrderIdView, const TradeContext &), (override));
  MOCK_METHOD(OrderInfo, queryOrderInfo, (OrderIdView, const TradeContext &), (override));
  MOCK_METHOD(InitiatedWithdrawInfo, launchWithdraw, (MonetaryAmount, Wallet &&), (override));
  MOCK_METHOD(MonetaryAmount, queryWithdrawDelivery, (const InitiatedWithdrawInfo &, const SentWithdrawInfo &),
              (override));
};

}  // namespace cct::api