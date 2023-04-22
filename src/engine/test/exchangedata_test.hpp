#pragma once

#include <gtest/gtest.h>

#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "exchange.hpp"
#include "exchangeprivateapi_mock.hpp"
#include "exchangepublicapi_mock.hpp"
#include "exchangepublicapitypes.hpp"

namespace cct {

class ExchangesBaseTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(exchangePrivate5, queryAccountBalance(testing::_)).WillRepeatedly(testing::Return(emptyBalance));
    EXPECT_CALL(exchangePrivate6, queryAccountBalance(testing::_)).WillRepeatedly(testing::Return(emptyBalance));
    EXPECT_CALL(exchangePrivate7, queryAccountBalance(testing::_)).WillRepeatedly(testing::Return(emptyBalance));
    EXPECT_CALL(exchangePrivate8, queryAccountBalance(testing::_)).WillRepeatedly(testing::Return(emptyBalance));
  }

  void TearDown() override {}

  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  settings::RunMode runMode = settings::RunMode::kTestKeys;
  CoincenterInfo coincenterInfo{runMode, loadConfiguration};
  api::CryptowatchAPI cryptowatchAPI{coincenterInfo, runMode, Duration::max(), true};
  FiatConverter fiatConverter{coincenterInfo, Duration::max()};  // max to avoid real Fiat converter queries
  api::MockExchangePublic exchangePublic1{kSupportedExchanges[0], fiatConverter, cryptowatchAPI, coincenterInfo};
  api::MockExchangePublic exchangePublic2{kSupportedExchanges[1], fiatConverter, cryptowatchAPI, coincenterInfo};
  api::MockExchangePublic exchangePublic3{kSupportedExchanges[2], fiatConverter, cryptowatchAPI, coincenterInfo};
  api::APIKey key1{"test1", "testuser1", "", "", ""};
  api::APIKey key2{"test2", "testuser2", "", "", ""};
  api::APIKey key3{"test3", "testuser3", "", "", ""};
  api::APIKey key4{"test4", "testuser4", "", "", ""};
  api::APIKey key5{"test5", "testuser5", "", "", ""};
  api::MockExchangePrivate exchangePrivate1{exchangePublic1, coincenterInfo, key1};
  api::MockExchangePrivate exchangePrivate2{exchangePublic2, coincenterInfo, key1};
  api::MockExchangePrivate exchangePrivate3{exchangePublic3, coincenterInfo, key1};
  api::MockExchangePrivate exchangePrivate4{exchangePublic3, coincenterInfo, key2};
  api::MockExchangePrivate exchangePrivate5{exchangePublic3, coincenterInfo, key3};
  api::MockExchangePrivate exchangePrivate6{exchangePublic3, coincenterInfo, key4};
  api::MockExchangePrivate exchangePrivate7{exchangePublic3, coincenterInfo, key5};
  api::MockExchangePrivate exchangePrivate8{exchangePublic1, coincenterInfo, key2};
  Exchange exchange1{coincenterInfo.exchangeInfo(exchangePublic1.name()), exchangePublic1, exchangePrivate1};
  Exchange exchange2{coincenterInfo.exchangeInfo(exchangePublic2.name()), exchangePublic2, exchangePrivate2};
  Exchange exchange3{coincenterInfo.exchangeInfo(exchangePublic3.name()), exchangePublic3, exchangePrivate3};
  Exchange exchange4{coincenterInfo.exchangeInfo(exchangePublic3.name()), exchangePublic3, exchangePrivate4};
  Exchange exchange5{coincenterInfo.exchangeInfo(exchangePublic3.name()), exchangePublic3, exchangePrivate5};
  Exchange exchange6{coincenterInfo.exchangeInfo(exchangePublic3.name()), exchangePublic3, exchangePrivate6};
  Exchange exchange7{coincenterInfo.exchangeInfo(exchangePublic3.name()), exchangePublic3, exchangePrivate7};
  Exchange exchange8{coincenterInfo.exchangeInfo(exchangePublic1.name()), exchangePublic1, exchangePrivate8};

  Market m1{"ETH", "EUR"};
  Market m2{"BTC", "EUR"};
  Market m3{"XRP", "BTC"};

  VolAndPriNbDecimals volAndPriDec1{2, 2};
  int depth = 10;
  int64_t nbSecondsSinceEpoch = 0;

  MonetaryAmount askPrice1{"2300.45EUR"};
  MonetaryAmount bidPrice1{"2300.4EUR"};
  MarketOrderBook marketOrderBook10{
      askPrice1, MonetaryAmount("1.09ETH"), bidPrice1, MonetaryAmount("41ETH"), volAndPriDec1, depth};
  MarketOrderBook marketOrderBook11{MonetaryAmount{"2301.15EUR"},
                                    MonetaryAmount("0.4ETH"),
                                    MonetaryAmount{"2301.05EUR"},
                                    MonetaryAmount("17ETH"),
                                    volAndPriDec1,
                                    depth - 2};

  VolAndPriNbDecimals volAndPriDec2{5, 2};
  MonetaryAmount askPrice2{"31056.67 EUR"};
  MonetaryAmount bidPrice2{"31056.66 EUR"};
  MarketOrderBook marketOrderBook20{
      askPrice2, MonetaryAmount("0.12BTC"), bidPrice2, MonetaryAmount("0.00234 BTC"), volAndPriDec2, depth};
  MarketOrderBook marketOrderBook21{MonetaryAmount{"31051.02 EUR"},
                                    MonetaryAmount("0.409BTC"),
                                    MonetaryAmount{"31051.01 EUR"},
                                    MonetaryAmount("1.9087 BTC"),
                                    volAndPriDec2,
                                    depth + 1};

  VolAndPriNbDecimals volAndPriDec3{1, 2};
  MonetaryAmount askPrice3{"0.37 BTC"};
  MonetaryAmount bidPrice3{"0.36 BTC"};
  MarketOrderBook marketOrderBook3{
      askPrice3, MonetaryAmount("916.4XRP"), bidPrice3, MonetaryAmount("3494XRP"), volAndPriDec3, depth};

  const MonetaryAmount amounts1[4] = {MonetaryAmount("1500XRP"), MonetaryAmount("15BTC"), MonetaryAmount("1.5ETH"),
                                      MonetaryAmount("5000USDT")};
  const MonetaryAmount amounts2[4] = {MonetaryAmount("37SOL"), MonetaryAmount("1887565SHIB"), MonetaryAmount("0.5BTC"),
                                      MonetaryAmount("6750USDT")};
  const MonetaryAmount amounts3[5] = {MonetaryAmount("0.6ETH"), MonetaryAmount("1000XLM"), MonetaryAmount("0.01AVAX"),
                                      MonetaryAmount("1500EUR"), MonetaryAmount("4250USDT")};
  const MonetaryAmount amounts4[6] = {MonetaryAmount("147ADA"),     MonetaryAmount("4.76DOT"),
                                      MonetaryAmount("15004MATIC"), MonetaryAmount("155USD"),
                                      MonetaryAmount("107.5USDT"),  MonetaryAmount("1200EUR")};

  BalancePortfolio balancePortfolio1{amounts1};
  BalancePortfolio balancePortfolio2{amounts2};
  BalancePortfolio balancePortfolio3{amounts3};
  BalancePortfolio balancePortfolio4{amounts4};
  BalancePortfolio emptyBalance;
};
}  // namespace cct