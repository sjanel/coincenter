#pragma once

#include <gtest/gtest.h>

#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "exchange.hpp"
#include "exchangeprivateapi_mock.hpp"
#include "exchangepublicapi_mock.hpp"
#include "timedef.hpp"

namespace cct {

class ExchangesBaseTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(exchangePrivate5, queryAccountBalance(testing::_)).WillRepeatedly(testing::Return(emptyBalance));
    EXPECT_CALL(exchangePrivate6, queryAccountBalance(testing::_)).WillRepeatedly(testing::Return(emptyBalance));
    EXPECT_CALL(exchangePrivate7, queryAccountBalance(testing::_)).WillRepeatedly(testing::Return(emptyBalance));
    EXPECT_CALL(exchangePrivate8, queryAccountBalance(testing::_)).WillRepeatedly(testing::Return(emptyBalance));
  }

  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  settings::RunMode runMode = settings::RunMode::kTestKeys;
  CoincenterInfo coincenterInfo{runMode, loadConfiguration};
  api::CommonAPI commonAPI{coincenterInfo, Duration::max()};
  FiatConverter fiatConverter{coincenterInfo, Duration::max()};  // max to avoid real Fiat converter queries
  api::MockExchangePublic exchangePublic1{kSupportedExchanges[0], fiatConverter, commonAPI, coincenterInfo};
  api::MockExchangePublic exchangePublic2{kSupportedExchanges[1], fiatConverter, commonAPI, coincenterInfo};
  api::MockExchangePublic exchangePublic3{kSupportedExchanges[2], fiatConverter, commonAPI, coincenterInfo};
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
  Exchange exchange1{coincenterInfo.exchangeConfig(exchangePublic1.name()), exchangePublic1, exchangePrivate1};
  Exchange exchange2{coincenterInfo.exchangeConfig(exchangePublic2.name()), exchangePublic2, exchangePrivate2};
  Exchange exchange3{coincenterInfo.exchangeConfig(exchangePublic3.name()), exchangePublic3, exchangePrivate3};
  Exchange exchange4{coincenterInfo.exchangeConfig(exchangePublic3.name()), exchangePublic3, exchangePrivate4};
  Exchange exchange5{coincenterInfo.exchangeConfig(exchangePublic3.name()), exchangePublic3, exchangePrivate5};
  Exchange exchange6{coincenterInfo.exchangeConfig(exchangePublic3.name()), exchangePublic3, exchangePrivate6};
  Exchange exchange7{coincenterInfo.exchangeConfig(exchangePublic3.name()), exchangePublic3, exchangePrivate7};
  Exchange exchange8{coincenterInfo.exchangeConfig(exchangePublic1.name()), exchangePublic1, exchangePrivate8};

  static constexpr Market m1{"ETH", "EUR"};
  static constexpr Market m2{"BTC", "EUR"};
  static constexpr Market m3{"XRP", "BTC"};

  static constexpr VolAndPriNbDecimals volAndPriDec1{2, 2};
  static constexpr int depth = 10;

  static constexpr MonetaryAmount askPrice1{230045, "EUR", 2};
  static constexpr MonetaryAmount bidPrice1{23004, "EUR", 1};

  TimePoint time{};

  MarketOrderBook marketOrderBook10{
      time, askPrice1, MonetaryAmount(109, "ETH", 2), bidPrice1, MonetaryAmount(41, "ETH"), volAndPriDec1, depth};
  MarketOrderBook marketOrderBook11{time,
                                    MonetaryAmount{"2301.15EUR"},
                                    MonetaryAmount("0.4ETH"),
                                    MonetaryAmount{"2301.05EUR"},
                                    MonetaryAmount("17ETH"),
                                    volAndPriDec1,
                                    depth - 2};

  static constexpr VolAndPriNbDecimals volAndPriDec2{5, 2};
  static constexpr MonetaryAmount askPrice2{3105667, "EUR", 2};
  static constexpr MonetaryAmount bidPrice2{3105666, "EUR", 2};
  MarketOrderBook marketOrderBook20{
      time, askPrice2, MonetaryAmount(12, "BTC", 2), bidPrice2, MonetaryAmount(234, "BTC", 5), volAndPriDec2, depth};
  MarketOrderBook marketOrderBook21{time,
                                    MonetaryAmount{3105102, "EUR", 2},
                                    MonetaryAmount(409, "BTC", 3),
                                    MonetaryAmount{3105101, "EUR", 2},
                                    MonetaryAmount(19087, "BTC", 4),
                                    volAndPriDec2,
                                    depth + 1};

  static constexpr VolAndPriNbDecimals volAndPriDec3{1, 2};
  static constexpr MonetaryAmount askPrice3{37, "BTC", 2};
  static constexpr MonetaryAmount bidPrice3{36, "BTC", 2};
  MarketOrderBook marketOrderBook3{
      time, askPrice3, MonetaryAmount(9164, "XRP", 1), bidPrice3, MonetaryAmount(3494, "XRP"), volAndPriDec3, depth};

  static constexpr MonetaryAmount amounts1[] = {MonetaryAmount(1500, "XRP"), MonetaryAmount(15, "BTC"),
                                                MonetaryAmount(15, "ETH", 1), MonetaryAmount(5000, "USDT")};
  static constexpr MonetaryAmount amounts2[] = {MonetaryAmount(37, "SOL"), MonetaryAmount(1887565, "SHIB"),
                                                MonetaryAmount(5, "BTC", 1), MonetaryAmount(6750, "USDT")};
  static constexpr MonetaryAmount amounts3[] = {MonetaryAmount(6, "ETH", 1), MonetaryAmount(1000, "XLM"),
                                                MonetaryAmount(1, "AVAX", 2), MonetaryAmount(1500, "EUR"),
                                                MonetaryAmount(4250, "USDT")};
  static constexpr MonetaryAmount amounts4[] = {MonetaryAmount(147, "ADA"),      MonetaryAmount(476, "DOT", 2),
                                                MonetaryAmount(15004, "MATIC"),  MonetaryAmount(155, "USD"),
                                                MonetaryAmount(1075, "USDT", 1), MonetaryAmount(1200, "EUR")};

  BalancePortfolio balancePortfolio1{amounts1};
  BalancePortfolio balancePortfolio2{amounts2};
  BalancePortfolio balancePortfolio3{amounts3};
  BalancePortfolio balancePortfolio4{amounts4};
  BalancePortfolio emptyBalance;
};
}  // namespace cct
