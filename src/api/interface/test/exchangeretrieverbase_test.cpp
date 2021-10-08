#include "exchangeretrieverbase.hpp"

#include <gtest/gtest.h>

namespace cct {

class ExchangeTest {
 public:
  using ExchangePublic = ExchangeTest;

  ExchangeTest(std::string_view n, std::string_view kn) : _name(n), _keyName(kn) {}

  std::string_view name() const { return _name; }
  std::string_view keyName() const { return _keyName; }

  ExchangePublic &apiPublic() { return *this; }
  const ExchangePublic &apiPublic() const { return *this; }

 private:
  std::string_view _name;
  std::string_view _keyName;
};

using ExchangeRetriever = ExchangeRetrieverBase<ExchangeTest>;
using Order = ExchangeRetriever::Order;

TEST(ExchangeRetriever, Empty) {
  EXPECT_TRUE(ExchangeRetriever().exchanges().empty());
  PublicExchangeNames names;
  EXPECT_TRUE(ExchangeRetriever().retrieveSelectedExchanges(Order::kInitial, names).empty());
}

TEST(ExchangeRetriever, RetrieveUniqueCandidate) {
  ExchangeTest kAllExchanges[] = {ExchangeTest("bithumb", "user1"), ExchangeTest("kraken", "user3"),
                                  ExchangeTest("bithumb", "user2")};
  ExchangeRetriever exchangeRetriever(kAllExchanges);
  // ambiguity, should throw exception
  EXPECT_THROW(exchangeRetriever.retrieveUniqueCandidate(PrivateExchangeName("bithumb")), exception);
  ExchangeTest &bithumbUser1 = exchangeRetriever.retrieveUniqueCandidate(PrivateExchangeName("bithumb_user1"));
  EXPECT_EQ(bithumbUser1.name(), "bithumb");
  EXPECT_EQ(bithumbUser1.keyName(), "user1");
  ExchangeTest &krakenUser1 = exchangeRetriever.retrieveUniqueCandidate(PrivateExchangeName("kraken"));
  EXPECT_EQ(krakenUser1.name(), "kraken");
  EXPECT_EQ(krakenUser1.keyName(), "user3");
}

TEST(ExchangeRetriever, RetrieveSelectedExchangesInitialOrder) {
  ExchangeTest kAllExchanges[] = {ExchangeTest("kraken", "user1"), ExchangeTest("bithumb", "user1"),
                                  ExchangeTest("kraken", "user2")};
  ExchangeRetriever exchangeRetriever(kAllExchanges);
  EXPECT_FALSE(exchangeRetriever.exchanges().empty());
  PublicExchangeNames names{"kraken"};
  ExchangeRetriever::SelectedExchanges selectedExchanges =
      exchangeRetriever.retrieveSelectedExchanges(Order::kInitial, names);
  ASSERT_EQ(selectedExchanges.size(), 2U);
  EXPECT_EQ(selectedExchanges.front()->name(), "kraken");
  EXPECT_EQ(selectedExchanges.back()->name(), "kraken");
  selectedExchanges = exchangeRetriever.retrieveSelectedExchanges(Order::kInitial);
  ASSERT_EQ(selectedExchanges.size(), 3U);
  EXPECT_EQ(selectedExchanges[0]->name(), "kraken");
  EXPECT_EQ(selectedExchanges[1]->name(), "bithumb");
  EXPECT_EQ(selectedExchanges[2]->name(), "kraken");
}

TEST(ExchangeRetriever, RetrieveSelectedExchangesSelectedOrder) {
  const std::pair<std::string, std::string> kExchangePairs[] = {{"kraken", "bithumb"}, {"bithumb", "kraken"}};
  for (const auto &[first, second] : kExchangePairs) {
    ExchangeTest kAllExchanges[] = {ExchangeTest(first, "user1"), ExchangeTest(second, "user1"),
                                    ExchangeTest(first, "user2")};
    ExchangeRetriever exchangeRetriever(kAllExchanges);
    PublicExchangeNames names{second, first};
    ExchangeRetriever::SelectedExchanges selectedExchanges =
        exchangeRetriever.retrieveSelectedExchanges(Order::kSelection, names);
    ASSERT_EQ(selectedExchanges.size(), 3U);
    EXPECT_EQ(selectedExchanges[0]->name(), second);
    EXPECT_EQ(selectedExchanges[1]->name(), first);
    EXPECT_EQ(selectedExchanges[2]->name(), first);
  }
}

TEST(ExchangeRetriever, RetrieveAtMostOneAccountSelectedExchanges) {
  const std::pair<std::string, std::string> kExchangePairs[] = {{"kraken", "bithumb"}, {"bithumb", "kraken"}};
  for (const auto &[first, second] : kExchangePairs) {
    ExchangeTest kAllExchanges[] = {ExchangeTest(first, "user1"), ExchangeTest(second, "user1"),
                                    ExchangeTest(first, "user2")};
    ExchangeRetriever exchangeRetriever(kAllExchanges);
    PublicExchangeNames names{second, first};
    ExchangeRetriever::UniquePublicSelectedExchanges selectedExchanges =
        exchangeRetriever.retrieveAtMostOneAccountSelectedExchanges(names);
    ASSERT_EQ(selectedExchanges.size(), 2U);
    EXPECT_EQ(selectedExchanges.front()->name(), second);
    EXPECT_EQ(selectedExchanges.back()->name(), first);
  }
}

TEST(ExchangeRetriever, RetrieveUniquePublicExchange) {
  const std::pair<std::string, std::string> kExchangePairs[] = {{"kraken", "bithumb"}, {"bithumb", "kraken"}};
  for (const auto &[first, second] : kExchangePairs) {
    ExchangeTest kAllExchanges[] = {ExchangeTest(first, "user1"), ExchangeTest(second, "user1")};
    ExchangeRetriever exchangeRetriever(kAllExchanges);
    PublicExchangeNames names{second, first};
    ExchangeRetriever::UniquePublicExchanges selectedExchanges = exchangeRetriever.retrieveUniquePublicExchanges(names);
    ASSERT_EQ(selectedExchanges.size(), 2U);
    EXPECT_EQ(selectedExchanges.front()->name(), second);
    EXPECT_EQ(selectedExchanges.back()->name(), first);
  }
}
}  // namespace cct