#include "exchangeretrieverbase.hpp"

#include <gtest/gtest.h>

namespace cct {

class ExchangeTest {
 public:
  using ExchangePublic = ExchangeTest;

  ExchangeTest(std::string_view name, std::string_view keyName) : _name(name), _keyName(keyName) {}

  std::string_view name() const { return _name; }
  std::string_view keyName() const { return _keyName; }

  ExchangePublic &apiPublic() { return *this; }
  const ExchangePublic &apiPublic() const { return *this; }

  bool matches(const ExchangeName &exchangeName) const {
    return exchangeName.name() == _name && (!exchangeName.isKeyNameDefined() || exchangeName.keyName() == _keyName);
  }

 private:
  std::string_view _name;
  std::string_view _keyName;
};

using ExchangeRetriever = ExchangeRetrieverBase<const ExchangeTest>;
using Order = ExchangeRetriever::Order;

TEST(ExchangeRetriever, Empty) {
  EXPECT_TRUE(ExchangeRetriever().exchanges().empty());
  ExchangeNames names;
  EXPECT_TRUE(ExchangeRetriever().select(Order::kInitial, names).empty());
}

TEST(ExchangeRetriever, RetrieveUniqueCandidate) {
  const ExchangeTest kAllExchanges[] = {ExchangeTest("bithumb", "user1"), ExchangeTest("kraken", "user3"),
                                        ExchangeTest("bithumb", "user2")};
  ExchangeRetriever exchangeRetriever(kAllExchanges);
  // ambiguity, should throw exception
  EXPECT_THROW(exchangeRetriever.retrieveUniqueCandidate(ExchangeName("bithumb")), exception);
  const ExchangeTest &bithumbUser1 = exchangeRetriever.retrieveUniqueCandidate(ExchangeName("bithumb_user1"));
  EXPECT_EQ(bithumbUser1.name(), "bithumb");
  EXPECT_EQ(bithumbUser1.keyName(), "user1");
  const ExchangeTest &krakenUser1 = exchangeRetriever.retrieveUniqueCandidate(ExchangeName("kraken"));
  EXPECT_EQ(krakenUser1.name(), "kraken");
  EXPECT_EQ(krakenUser1.keyName(), "user3");
}

TEST(ExchangeRetriever, RetrieveSelectedExchangesInitialOrder) {
  const ExchangeTest kAllExchanges[] = {ExchangeTest("kraken", "user1"), ExchangeTest("bithumb", "user1"),
                                        ExchangeTest("kraken", "user2")};
  ExchangeRetriever exchangeRetriever(kAllExchanges);
  EXPECT_FALSE(exchangeRetriever.exchanges().empty());
  ExchangeName krakenExchangeName("kraken");
  ExchangeNames names{krakenExchangeName};
  ExchangeRetriever::SelectedExchanges selectedExchanges = exchangeRetriever.select(Order::kInitial, names);
  ASSERT_EQ(selectedExchanges.size(), 2U);
  EXPECT_EQ(selectedExchanges.front()->name(), "kraken");
  EXPECT_EQ(selectedExchanges.back()->name(), "kraken");
  selectedExchanges = exchangeRetriever.select(Order::kInitial, ExchangeNames());
  ASSERT_EQ(selectedExchanges.size(), 3U);
  EXPECT_EQ(selectedExchanges[0]->name(), "kraken");
  EXPECT_EQ(selectedExchanges[1]->name(), "bithumb");
  EXPECT_EQ(selectedExchanges[2]->name(), "kraken");
}

TEST(ExchangeRetriever, RetrieveSelectedExchangesSelectedOrder) {
  const std::pair<string, string> kExchangePairs[] = {{"kraken", "bithumb"}, {"bithumb", "kraken"}};
  for (const auto &[first, second] : kExchangePairs) {
    const ExchangeTest kAllExchanges[] = {ExchangeTest(first, "user1"), ExchangeTest(second, "user1"),
                                          ExchangeTest(first, "user2")};
    ExchangeRetriever exchangeRetriever(kAllExchanges);
    ExchangeNames names{ExchangeName(second), ExchangeName(first)};
    ExchangeRetriever::SelectedExchanges selectedExchanges = exchangeRetriever.select(Order::kSelection, names);
    ASSERT_EQ(selectedExchanges.size(), 3U);
    EXPECT_EQ(selectedExchanges[0]->name(), second);
    EXPECT_EQ(selectedExchanges[1]->name(), first);
    EXPECT_EQ(selectedExchanges[2]->name(), first);
  }
}

TEST(ExchangeRetriever, RetrieveAtMostOneAccountSelectedExchanges) {
  const std::pair<string, string> kExchangePairs[] = {{"kraken", "bithumb"}, {"bithumb", "kraken"}};
  for (const auto &[first, second] : kExchangePairs) {
    const ExchangeTest kAllExchanges[] = {ExchangeTest(first, "user1"), ExchangeTest(second, "user1"),
                                          ExchangeTest(first, "user2")};
    ExchangeRetriever exchangeRetriever(kAllExchanges);
    ExchangeNames names{ExchangeName(second), ExchangeName(first)};
    ExchangeRetriever::UniquePublicSelectedExchanges selectedExchanges = exchangeRetriever.selectOneAccount(names);
    ASSERT_EQ(selectedExchanges.size(), 2U);
    EXPECT_EQ(selectedExchanges.front()->name(), second);
    EXPECT_EQ(selectedExchanges.back()->name(), first);
  }
}

TEST(ExchangeRetriever, RetrieveUniquePublicExchange) {
  const std::pair<string, string> kExchangePairs[] = {{"kraken", "bithumb"}, {"bithumb", "kraken"}};
  for (const auto &[first, second] : kExchangePairs) {
    const ExchangeTest kAllExchanges[] = {ExchangeTest(first, "user1"), ExchangeTest(second, "user1")};
    ExchangeRetriever exchangeRetriever(kAllExchanges);
    ExchangeNames names{ExchangeName(second), ExchangeName(first)};
    ExchangeRetriever::PublicExchangesVec selectedExchanges = exchangeRetriever.selectPublicExchanges(names);
    ASSERT_EQ(selectedExchanges.size(), 2U);
    EXPECT_EQ(selectedExchanges.front()->name(), second);
    EXPECT_EQ(selectedExchanges.back()->name(), first);
  }
}
}  // namespace cct