#include "exchangeretriever.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <span>
#include <string_view>

#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "exchange.hpp"
#include "exchangeconfigmap.hpp"
#include "exchangeconfigparser.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi_mock.hpp"
#include "exchangepublicapi_mock.hpp"
#include "fiatconverter.hpp"
#include "loadconfiguration.hpp"
#include "reader.hpp"
#include "timedef.hpp"

namespace cct {

class ExchangeRetrieverTest : public ::testing::Test {
 protected:
  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  ExchangeConfigMap exchangeConfigMap{
      ComputeExchangeConfigMap(loadConfiguration.exchangeConfigFileName(), LoadExchangeConfigData(loadConfiguration))};

  CoincenterInfo coincenterInfo{settings::RunMode::kTestKeys, loadConfiguration};
  api::CommonAPI commonAPI{coincenterInfo, Duration::max()};
  FiatConverter fiatConverter{coincenterInfo, Duration::max(), Reader()};  // max to avoid real Fiat converter queries

  api::MockExchangePublic exchangePublic1{"bithumb", fiatConverter, commonAPI, coincenterInfo};
  api::MockExchangePublic exchangePublic2{"kraken", fiatConverter, commonAPI, coincenterInfo};
  api::MockExchangePublic exchangePublic3{"kucoin", fiatConverter, commonAPI, coincenterInfo};
  api::APIKey key1{"test1", "user1", "", "", ""};
  api::APIKey key2{"test2", "user2", "", "", ""};
  api::APIKey key3{"test3", "user3", "", "", ""};
  api::APIKey key4{"test4", "user4", "", "", ""};
  api::APIKey key5{"test5", "user5", "", "", ""};
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
  Exchange exchange7{coincenterInfo.exchangeConfig(exchangePublic3.name()), exchangePublic3};
  Exchange exchange8{coincenterInfo.exchangeConfig(exchangePublic1.name()), exchangePublic1, exchangePrivate8};
};

TEST_F(ExchangeRetrieverTest, Empty) {
  EXPECT_TRUE(ExchangeRetriever().exchanges().empty());
  EXPECT_TRUE(ExchangeRetriever().select(ExchangeRetriever::Order::kInitial, ExchangeNames{}).empty());
}

TEST_F(ExchangeRetrieverTest, EmptySelection) {
  Exchange kAllExchanges[] = {exchange1, exchange2, exchange7, exchange8};

  ExchangeRetriever exchangeRetriever(kAllExchanges);

  EXPECT_FALSE(exchangeRetriever.exchanges().empty());

  ExchangeRetriever::SelectedExchanges expected;
  std::ranges::transform(kAllExchanges, std::back_inserter(expected),
                         [](auto &exchange) { return std::addressof(exchange); });

  EXPECT_EQ(exchangeRetriever.select(ExchangeRetriever::Order::kInitial, ExchangeNames{}), expected);
  EXPECT_EQ(exchangeRetriever.select(ExchangeRetriever::Order::kSelection, ExchangeNames{}), expected);
}

TEST_F(ExchangeRetrieverTest, RetrieveUniqueCandidate) {
  Exchange kAllExchanges[] = {exchange1, exchange2, exchange7, exchange8};

  ExchangeRetriever exchangeRetriever(kAllExchanges);

  EXPECT_THROW(exchangeRetriever.retrieveUniqueCandidate(ExchangeName("bithumb")), exception);

  auto &bithumbUser1 = exchangeRetriever.retrieveUniqueCandidate(ExchangeName("bithumb_user1"));

  EXPECT_EQ(bithumbUser1.name(), "bithumb");
  EXPECT_EQ(bithumbUser1.keyName(), "user1");

  auto &krakenUser1 = exchangeRetriever.retrieveUniqueCandidate(ExchangeName("kraken"));

  EXPECT_EQ(krakenUser1.name(), "kraken");
  EXPECT_EQ(krakenUser1.keyName(), "user1");
}

TEST_F(ExchangeRetrieverTest, RetrieveSelectedExchangesInitialOrder) {
  Exchange kAllExchanges[] = {exchange1, exchange2, exchange7, exchange8};

  ExchangeRetriever exchangeRetriever(kAllExchanges);

  EXPECT_FALSE(exchangeRetriever.exchanges().empty());

  ExchangeName exchangeName("bithumb");
  ExchangeNames names{exchangeName};
  ExchangeRetriever::SelectedExchanges selectedExchanges =
      exchangeRetriever.select(ExchangeRetriever::Order::kInitial, names);

  ASSERT_EQ(selectedExchanges.size(), 2U);
  EXPECT_EQ(selectedExchanges.front()->name(), "bithumb");
  EXPECT_EQ(selectedExchanges.back()->name(), "bithumb");

  selectedExchanges = exchangeRetriever.select(ExchangeRetriever::Order::kInitial, ExchangeNames{});

  ASSERT_EQ(selectedExchanges.size(), 4U);
  EXPECT_EQ(selectedExchanges[0]->name(), "bithumb");
  EXPECT_EQ(selectedExchanges[1]->name(), "kraken");
  EXPECT_EQ(selectedExchanges[2]->name(), "kucoin");
  EXPECT_EQ(selectedExchanges[3]->name(), "bithumb");
}

TEST_F(ExchangeRetrieverTest, RetrieveSelectedExchangesFilterWhenAccountNotPresent) {
  Exchange kAllExchanges[] = {exchange1, exchange2, exchange7, exchange8};

  ExchangeRetriever exchangeRetriever(kAllExchanges);

  ExchangeRetriever::SelectedExchanges selectedExchanges = exchangeRetriever.select(
      ExchangeRetriever::Order::kInitial, ExchangeNames{}, ExchangeRetriever::Filter::kWithAccountWhenEmpty);

  ASSERT_EQ(selectedExchanges.size(), 3U);
  EXPECT_EQ(selectedExchanges[0]->name(), "bithumb");
  EXPECT_EQ(selectedExchanges[1]->name(), "kraken");
  EXPECT_EQ(selectedExchanges[2]->name(), "bithumb");

  selectedExchanges =
      exchangeRetriever.select(ExchangeRetriever::Order::kInitial, ExchangeNames{ExchangeName{"kraken"}},
                               ExchangeRetriever::Filter::kWithAccountWhenEmpty);

  ASSERT_EQ(selectedExchanges.size(), 1U);
  EXPECT_EQ(selectedExchanges[0]->name(), "kraken");

  // should be returned anyway when asked explicitly
  selectedExchanges =
      exchangeRetriever.select(ExchangeRetriever::Order::kInitial, ExchangeNames{ExchangeName{"kucoin"}},
                               ExchangeRetriever::Filter::kWithAccountWhenEmpty);

  ASSERT_EQ(selectedExchanges.size(), 1U);
  EXPECT_EQ(selectedExchanges[0]->name(), "kucoin");
}

TEST_F(ExchangeRetrieverTest, RetrieveSelectedExchangesSelectedOrder) {
  Exchange kAllExchanges1[] = {exchange1, exchange2, exchange8};
  Exchange kAllExchanges2[] = {exchange8, exchange1, exchange2};

  std::span<Exchange> exchangesSpan1 = kAllExchanges1;
  std::span<Exchange> exchangesSpan2 = kAllExchanges2;

  for (auto exchangesSpan : {exchangesSpan1, exchangesSpan2}) {
    ExchangeRetriever exchangeRetriever(exchangesSpan);
    ExchangeNames names{ExchangeName("kraken"), ExchangeName("bithumb")};
    ExchangeRetriever::SelectedExchanges selectedExchanges =
        exchangeRetriever.select(ExchangeRetriever::Order::kSelection, names);
    ASSERT_EQ(selectedExchanges.size(), 3U);
    EXPECT_EQ(selectedExchanges[0]->name(), "kraken");
    EXPECT_EQ(selectedExchanges[1]->name(), "bithumb");
    EXPECT_EQ(selectedExchanges[2]->name(), "bithumb");
  }
}

TEST_F(ExchangeRetrieverTest, RetrieveAtMostOneAccountSelectedExchanges) {
  Exchange kAllExchanges1[] = {exchange1, exchange2, exchange8};
  Exchange kAllExchanges2[] = {exchange8, exchange1, exchange2};

  std::span<Exchange> exchangesSpan1 = kAllExchanges1;
  std::span<Exchange> exchangesSpan2 = kAllExchanges2;

  for (auto exchangesSpan : {exchangesSpan1, exchangesSpan2}) {
    ExchangeRetriever exchangeRetriever(exchangesSpan);

    ExchangeNames names{ExchangeName("kraken"), ExchangeName("bithumb")};
    ExchangeRetriever::UniquePublicSelectedExchanges selectedExchanges = exchangeRetriever.selectOneAccount(names);
    ExchangeRetriever::UniquePublicSelectedExchanges exchangesInitialOrder =
        exchangeRetriever.selectOneAccount(ExchangeNames{});

    ASSERT_EQ(selectedExchanges.size(), 2U);
    EXPECT_EQ(selectedExchanges.front()->name(), "kraken");
    EXPECT_EQ(selectedExchanges.back()->name(), "bithumb");

    ASSERT_EQ(exchangesInitialOrder.size(), 2U);
    EXPECT_EQ(exchangesInitialOrder.front()->name(), "bithumb");
    EXPECT_EQ(exchangesInitialOrder.back()->name(), "kraken");
  }
}

TEST_F(ExchangeRetrieverTest, RetrieveUniquePublicExchange) {
  Exchange kAllExchanges1[] = {exchange1, exchange2, exchange8};
  Exchange kAllExchanges2[] = {exchange8, exchange1, exchange2};

  std::span<Exchange> exchangesSpan1 = kAllExchanges1;
  std::span<Exchange> exchangesSpan2 = kAllExchanges2;

  for (auto exchangesSpan : {exchangesSpan1, exchangesSpan2}) {
    ExchangeRetriever exchangeRetriever(exchangesSpan);
    ExchangeNames names{ExchangeName("kraken"), ExchangeName("bithumb")};
    ExchangeRetriever::PublicExchangesVec selectedExchanges = exchangeRetriever.selectPublicExchanges(names);

    ASSERT_EQ(selectedExchanges.size(), 2U);
    EXPECT_EQ(selectedExchanges.front()->name(), "kraken");
    EXPECT_EQ(selectedExchanges.back()->name(), "bithumb");
  }
}
}  // namespace cct