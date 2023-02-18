#pragma once

#include <gmock/gmock.h>

#include "cryptowatchapi.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"

namespace cct::api {
class MockExchangePublic : public ExchangePublic {
 public:
  MockExchangePublic(std::string_view name, FiatConverter &fiatConverter, CryptowatchAPI &cryptowatchApi,
                     const CoincenterInfo &config)
      : ExchangePublic(name, fiatConverter, cryptowatchApi, config) {}

  MOCK_METHOD(bool, healthCheck, (), (override));
  MOCK_METHOD(CurrencyExchangeFlatSet, queryTradableCurrencies, (), (override));
  MOCK_METHOD(CurrencyExchange, convertStdCurrencyToCurrencyExchange, (CurrencyCode currencyCode), (override));
  MOCK_METHOD(MarketSet, queryTradableMarkets, (), (override));
  MOCK_METHOD(MarketPriceMap, queryAllPrices, (), (override));
  MOCK_METHOD(WithdrawalFeeMap, queryWithdrawalFees, (), (override));
  MOCK_METHOD(MonetaryAmount, queryWithdrawalFee, (CurrencyCode currencyCode), (override));
  MOCK_METHOD(bool, isWithdrawalFeesSourceReliable, (), (const override));
  MOCK_METHOD(MarketOrderBookMap, queryAllApproximatedOrderBooks, (int depth), (override));
  MOCK_METHOD(MarketOrderBook, queryOrderBook, (Market mk, int depth), (override));
  MOCK_METHOD(MonetaryAmount, queryLast24hVolume, (Market mk), (override));
  MOCK_METHOD(LastTradesVector, queryLastTrades, (Market mk, int nbTrades), (override));
  MOCK_METHOD(MonetaryAmount, queryLastPrice, (Market mk), (override));
};
}  // namespace cct::api