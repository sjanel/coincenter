#include "exchangeretriever.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

#include "apikey.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "exchange-names.hpp"
#include "exchange.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi_mock.hpp"
#include "exchangepublicapi_mock.hpp"
#include "fiatconverter.hpp"
#include "loadconfiguration.hpp"
#include "reader.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace cct {

class ExchangeRetrieverTest : public ::testing::Test {
 protected:
  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  CoincenterInfo coincenterInfo{settings::RunMode::kTestKeys, loadConfiguration};

  api::CommonAPI commonAPI{coincenterInfo, Duration::max()};
  // max to avoid real Fiat converter queries
  FiatConverter fiatConverter{coincenterInfo, Duration::max(), Reader(), Reader()};

  api::MockExchangePublic exchangePublic1{ExchangeNameEnum::bithumb, fiatConverter, commonAPI, coincenterInfo};
  api::MockExchangePublic exchangePublic2{ExchangeNameEnum::kraken, fiatConverter, commonAPI, coincenterInfo};
  api::MockExchangePublic exchangePublic3{ExchangeNameEnum::kucoin, fiatConverter, commonAPI, coincenterInfo};
  api::APIKey key1{"user1", "", "", ""};
  api::APIKey key2{"user2", "", "", ""};
  Exchange exchange1{coincenterInfo.exchangeConfig(exchangePublic1.exchangeNameEnum()), exchangePublic1,
                     std::make_unique<api::MockExchangePrivate>(exchangePublic1, coincenterInfo, key1)};
  Exchange exchange2{coincenterInfo.exchangeConfig(exchangePublic2.exchangeNameEnum()), exchangePublic2,
                     std::make_unique<api::MockExchangePrivate>(exchangePublic2, coincenterInfo, key1)};
  Exchange exchange3{coincenterInfo.exchangeConfig(exchangePublic3.exchangeNameEnum()), exchangePublic3};
  Exchange exchange4{coincenterInfo.exchangeConfig(exchangePublic1.exchangeNameEnum()), exchangePublic1,
                     std::make_unique<api::MockExchangePrivate>(exchangePublic1, coincenterInfo, key2)};
};

TEST_F(ExchangeRetrieverTest, Empty) {
  EXPECT_TRUE(ExchangeRetriever().exchanges().empty());
  EXPECT_TRUE(ExchangeRetriever().select(ExchangeRetriever::Order::kInitial, ExchangeNames{}).empty());
}

TEST_F(ExchangeRetrieverTest, EmptySelection) {
  Exchange kAllExchanges[] = {std::move(exchange1), std::move(exchange2), std::move(exchange3), std::move(exchange4)};

  ExchangeRetriever exchangeRetriever(kAllExchanges);

  EXPECT_FALSE(exchangeRetriever.exchanges().empty());

  ExchangeRetriever::SelectedExchanges expected;
  std::ranges::transform(kAllExchanges, std::back_inserter(expected),
                         [](auto &exchange) { return std::addressof(exchange); });

  EXPECT_EQ(exchangeRetriever.select(ExchangeRetriever::Order::kInitial, ExchangeNames{}), expected);
  EXPECT_EQ(exchangeRetriever.select(ExchangeRetriever::Order::kSelection, ExchangeNames{}), expected);
}

TEST_F(ExchangeRetrieverTest, RetrieveUniqueCandidate) {
  Exchange kAllExchanges[] = {std::move(exchange1), std::move(exchange2), std::move(exchange3), std::move(exchange4)};

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
  Exchange kAllExchanges[] = {std::move(exchange1), std::move(exchange2), std::move(exchange3), std::move(exchange4)};

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
  Exchange kAllExchanges[] = {std::move(exchange1), std::move(exchange2), std::move(exchange3), std::move(exchange4)};

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
  Exchange kAllExchanges[] = {std::move(exchange1), std::move(exchange2), std::move(exchange4)};

  for (int pos = 0; pos < 2; ++pos) {
    ExchangeRetriever exchangeRetriever(kAllExchanges);
    ExchangeNames names{ExchangeName("kraken"), ExchangeName("bithumb")};
    ExchangeRetriever::SelectedExchanges selectedExchanges =
        exchangeRetriever.select(ExchangeRetriever::Order::kSelection, names);
    ASSERT_EQ(selectedExchanges.size(), 3U);
    EXPECT_EQ(selectedExchanges[0]->name(), "kraken");
    EXPECT_EQ(selectedExchanges[1]->name(), "bithumb");
    EXPECT_EQ(selectedExchanges[2]->name(), "bithumb");

    std::ranges::rotate(kAllExchanges, std::begin(kAllExchanges) + 2);
  }
}

TEST_F(ExchangeRetrieverTest, RetrieveAtMostOneAccountSelectedExchanges) {
  Exchange kAllExchanges[] = {std::move(exchange1), std::move(exchange2), std::move(exchange4)};

  for (int pos = 0; pos < 2; ++pos) {
    ExchangeRetriever exchangeRetriever(kAllExchanges);

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

    std::ranges::rotate(kAllExchanges, std::begin(kAllExchanges) + 2);
  }
}

TEST_F(ExchangeRetrieverTest, RetrieveUniquePublicExchange) {
  Exchange kAllExchanges[] = {std::move(exchange1), std::move(exchange2), std::move(exchange4)};

  for (int pos = 0; pos < 2; ++pos) {
    ExchangeRetriever exchangeRetriever(kAllExchanges);
    ExchangeNames names{ExchangeName("kraken"), ExchangeName("bithumb")};
    ExchangeRetriever::PublicExchangesVec selectedExchanges = exchangeRetriever.selectPublicExchanges(names);

    ASSERT_EQ(selectedExchanges.size(), 2U);
    EXPECT_EQ(selectedExchanges.front()->name(), "kraken");
    EXPECT_EQ(selectedExchanges.back()->name(), "bithumb");

    std::ranges::rotate(kAllExchanges, std::begin(kAllExchanges) + 2);
  }
}
}  // namespace cct
