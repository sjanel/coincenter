#include <gtest/gtest.h>

#include "apikeysprovider.hpp"
#include "bithumbprivateapi.hpp"
#include "bithumbpublicapi.hpp"
#include "coincenterinfo.hpp"
#include "fiatconverter.hpp"

namespace cct::api {
class BithumbPrivateAPIPlaceOrderTest : public ::testing::Test {
 protected:
  PlaceOrderInfo placeOrder(MonetaryAmount volume, MonetaryAmount price, TradeSide tradeSide) {
    Market market{volume.currencyCode(), price.currencyCode()};
    TradeContext tradeContext{market, tradeSide};
    TradeOptions tradeOptions;
    TradeInfo tradeInfo{tradeContext, tradeOptions};

    return exchangePrivate.placeOrder(from, volume, price, tradeInfo);
  }

  void setOverridenQueryResponses(const std::map<string, string>& queryResponses) {
    exchangePrivate._curlHandle.setOverridenQueryResponses(queryResponses);
  }

  settings::RunMode runMode = settings::RunMode::kQueryResponseOverriden;
  LoadConfiguration loadConfig{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  CoincenterInfo coincenterInfo{runMode, loadConfig};
  FiatConverter fiatConverter{coincenterInfo, Duration::max()};  // max to avoid real Fiat converter queries
  CommonAPI commonAPI{coincenterInfo, Duration::max()};
  BithumbPublic exchangePublic{coincenterInfo, fiatConverter, commonAPI};
  APIKeysProvider apiKeysProvider{coincenterInfo.dataDir(), coincenterInfo.getRunMode()};
  ExchangeName exchangeName{exchangePublic.name(), apiKeysProvider.getKeyNames(exchangePublic.name()).front()};
  const APIKey& testKey = apiKeysProvider.get(exchangeName);
  BithumbPrivate exchangePrivate{coincenterInfo, exchangePublic, testKey};

  MonetaryAmount from;
};

TEST_F(BithumbPrivateAPIPlaceOrderTest, PlaceOrderShortenDecimals) {
  setOverridenQueryResponses(
      {/// Place order, with high number of decimals
       {"/trade/"
        "place?endpoint=%2Ftrade%2Fplace&order_currency=ETH&payment_currency=EUR&type=ask&price=1500&units=2.000001",
        R"({"status": "5600", "message":"수량은 소수점 4자"})"},
       /// Replace order with decimals correctly truncated
       {"/trade/"
        "place?endpoint=%2Ftrade%2Fplace&order_currency=ETH&payment_currency=EUR&type=ask&price=1500&units=2",
        R"({"status": "0000", "order_id": "ID0001"})"},
       /// Query once order info, order not matched
       {"/info/orders?endpoint=%2Finfo%2Forders&order_currency=ETH&payment_currency=EUR&type=ask&order_id=ID0001",
        R"({"status": "0000", "data": [{"order_id": "ID0001"}]})"}});

  PlaceOrderInfo placeOrderInfo =
      placeOrder(MonetaryAmount("2.000001ETH"), MonetaryAmount("1500EUR"), TradeSide::kSell);
  EXPECT_EQ(placeOrderInfo.orderId, "ID0001");
}

TEST_F(BithumbPrivateAPIPlaceOrderTest, NoPlaceOrderTooSmallAmount) {
  setOverridenQueryResponses(
      {/// Place order, with high number of decimals
       {"/trade/"
        "place?endpoint=%2Ftrade%2Fplace&order_currency=ETH&payment_currency=EUR&type=ask&price=1500&units=0.000001",
        R"({"status": "5600", "message":"수량은 소수점 4자"})"}});

  PlaceOrderInfo placeOrderInfo =
      placeOrder(MonetaryAmount("0.000001ETH"), MonetaryAmount("1500EUR"), TradeSide::kSell);
  EXPECT_EQ(placeOrderInfo.orderId, "UndefinedId");
}
}  // namespace cct::api