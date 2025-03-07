#pragma once

#include <gmock/gmock.h>

#include <optional>

#include "commonapi.hpp"
#include "exchange-name-enum.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "monetaryamount.hpp"
#include "public-trade-vector.hpp"

namespace cct::api {
class MockExchangePublic : public ExchangePublic {
 public:
  MockExchangePublic(ExchangeNameEnum exchangeNameEnum, FiatConverter &fiatConverter, CommonAPI &commonApi,
                     const CoincenterInfo &config)
      : ExchangePublic(exchangeNameEnum, fiatConverter, commonApi, config) {}

  MOCK_METHOD(bool, healthCheck, (), (override));
  MOCK_METHOD(CurrencyExchangeFlatSet, queryTradableCurrencies, (), (override));
  MOCK_METHOD(CurrencyExchange, convertStdCurrencyToCurrencyExchange, (CurrencyCode currencyCode), (override));
  MOCK_METHOD(MarketSet, queryTradableMarkets, (), (override));
  MOCK_METHOD(MarketPriceMap, queryAllPrices, (), (override));
  MOCK_METHOD(MonetaryAmountByCurrencySet, queryWithdrawalFees, (), (override));
  MOCK_METHOD(std::optional<MonetaryAmount>, queryWithdrawalFee, (CurrencyCode currencyCode), (override));
  MOCK_METHOD(bool, isWithdrawalFeesSourceReliable, (), (const override));
  MOCK_METHOD(MarketOrderBookMap, queryAllApproximatedOrderBooks, (int depth), (override));
  MOCK_METHOD(MarketOrderBook, queryOrderBook, (Market mk, int depth), (override));
  MOCK_METHOD(MonetaryAmount, queryLast24hVolume, (Market mk), (override));
  MOCK_METHOD(PublicTradeVector, queryLastTrades, (Market mk, int nbTrades), (override));
  MOCK_METHOD(MonetaryAmount, queryLastPrice, (Market mk), (override));
};
}  // namespace cct::api