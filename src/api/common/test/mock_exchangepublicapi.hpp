#pragma once

#include <gmock/gmock.h>

#include "cryptowatchapi.hpp"
#include "exchangepublicapi.hpp"
#include "fiatconverter.hpp"

namespace cct {
namespace api {
class MockExchangePublic : public ExchangePublic {
 public:
  MockExchangePublic(std::string_view name, FiatConverter &fiatConverter, CryptowatchAPI &cryptowatchApi,
                     const CoincenterInfo &config)
      : ExchangePublic(name, fiatConverter, cryptowatchApi, config) {}

  MOCK_METHOD(CurrencyExchangeFlatSet, queryTradableCurrencies, (), (override));
  MOCK_METHOD(CurrencyExchange, convertStdCurrencyToCurrencyExchange, (CurrencyCode currencyCode), (override));
  MOCK_METHOD(MarketSet, queryTradableMarkets, (), (override));
  MOCK_METHOD(MarketPriceMap, queryAllPrices, (), (override));
  MOCK_METHOD(WithdrawalFeeMap, queryWithdrawalFees, (), (override));
  MOCK_METHOD(MonetaryAmount, queryWithdrawalFee, (CurrencyCode currencyCode), (override));
  MOCK_METHOD(MarketOrderBookMap, queryAllApproximatedOrderBooks, (int depth), (override));
  MOCK_METHOD(MarketOrderBook, queryOrderBook, (Market m, int depth), (override));
  MOCK_METHOD(MonetaryAmount, queryLast24hVolume, (Market m), (override));
};
}  // namespace api
}  // namespace cct