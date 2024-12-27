#include "exchange-query-update-frequency-config.hpp"

#include <gtest/gtest.h>

namespace cct::schema {

class ExchangeQueryUpdateFrequencyConfigTest : public ::testing::Test {
 protected:
  ExchangeQueryUpdateFrequencyConfig data1{{QueryType::allOrderBooks, Duration{std::chrono::seconds(1)}},
                                           {QueryType::currencies, Duration{std::chrono::seconds(6)}},
                                           {QueryType::currencyInfo, Duration{std::chrono::seconds(3)}}};

  ExchangeQueryUpdateFrequencyConfig data2{{QueryType::allOrderBooks, Duration{std::chrono::seconds(4)}},
                                           {QueryType::withdrawalFees, Duration{std::chrono::seconds(5)}},
                                           {QueryType::currencies, Duration{std::chrono::seconds(2)}},
                                           {QueryType::orderBook, Duration{std::chrono::seconds(7)}}};
};

TEST_F(ExchangeQueryUpdateFrequencyConfigTest, MergeWith1) {
  ExchangeQueryUpdateFrequencyConfig des = data1;
  ExchangeQueryUpdateFrequencyConfig src = data2;
  MergeWith(src, des);
  EXPECT_EQ(des.size(), 5);
  EXPECT_EQ(des[0].first, QueryType::currencies);
  EXPECT_EQ(des[0].second.duration, std::chrono::seconds(2));
  EXPECT_EQ(des[1].first, QueryType::withdrawalFees);
  EXPECT_EQ(des[1].second.duration, std::chrono::seconds(5));
  EXPECT_EQ(des[2].first, QueryType::allOrderBooks);
  EXPECT_EQ(des[2].second.duration, std::chrono::seconds(1));
  EXPECT_EQ(des[3].first, QueryType::orderBook);
  EXPECT_EQ(des[3].second.duration, std::chrono::seconds(7));
  EXPECT_EQ(des[4].first, QueryType::currencyInfo);
  EXPECT_EQ(des[4].second.duration, std::chrono::seconds(3));
}

TEST_F(ExchangeQueryUpdateFrequencyConfigTest, MergeWith2) {
  ExchangeQueryUpdateFrequencyConfig des = data2;
  ExchangeQueryUpdateFrequencyConfig src = data1;
  MergeWith(src, des);
  EXPECT_EQ(des.size(), 5);
  EXPECT_EQ(des[0].first, QueryType::currencies);
  EXPECT_EQ(des[0].second.duration, std::chrono::seconds(2));
  EXPECT_EQ(des[1].first, QueryType::withdrawalFees);
  EXPECT_EQ(des[1].second.duration, std::chrono::seconds(5));
  EXPECT_EQ(des[2].first, QueryType::allOrderBooks);
  EXPECT_EQ(des[2].second.duration, std::chrono::seconds(1));
  EXPECT_EQ(des[3].first, QueryType::orderBook);
  EXPECT_EQ(des[3].second.duration, std::chrono::seconds(7));
  EXPECT_EQ(des[4].first, QueryType::currencyInfo);
  EXPECT_EQ(des[4].second.duration, std::chrono::seconds(3));
}

TEST_F(ExchangeQueryUpdateFrequencyConfigTest, MergeWithEmpty) {
  ExchangeQueryUpdateFrequencyConfig des = data1;
  ExchangeQueryUpdateFrequencyConfig src;
  MergeWith(src, des);
  EXPECT_EQ(des.size(), 3);
  EXPECT_EQ(des[0].first, QueryType::currencies);
  EXPECT_EQ(des[0].second.duration, std::chrono::seconds(6));
  EXPECT_EQ(des[1].first, QueryType::allOrderBooks);
  EXPECT_EQ(des[1].second.duration, std::chrono::seconds(1));
  EXPECT_EQ(des[2].first, QueryType::currencyInfo);
  EXPECT_EQ(des[2].second.duration, std::chrono::seconds(3));
}

TEST_F(ExchangeQueryUpdateFrequencyConfigTest, MergeWithEmptyDes) {
  ExchangeQueryUpdateFrequencyConfig des;
  ExchangeQueryUpdateFrequencyConfig src = data1;
  MergeWith(src, des);
  EXPECT_EQ(des.size(), 3);
  EXPECT_EQ(des[0].first, QueryType::currencies);
  EXPECT_EQ(des[0].second.duration, std::chrono::seconds(6));
  EXPECT_EQ(des[1].first, QueryType::allOrderBooks);
  EXPECT_EQ(des[1].second.duration, std::chrono::seconds(1));
  EXPECT_EQ(des[2].first, QueryType::currencyInfo);
  EXPECT_EQ(des[2].second.duration, std::chrono::seconds(3));
}

TEST_F(ExchangeQueryUpdateFrequencyConfigTest, MergeWithEmptyBoth) {
  ExchangeQueryUpdateFrequencyConfig des;
  ExchangeQueryUpdateFrequencyConfig src;
  MergeWith(src, des);
  EXPECT_EQ(des.size(), 0);
}

TEST_F(ExchangeQueryUpdateFrequencyConfigTest, MergeWithSame) {
  ExchangeQueryUpdateFrequencyConfig des = data1;
  ExchangeQueryUpdateFrequencyConfig src = data1;
  MergeWith(src, des);
  EXPECT_EQ(des.size(), 3);
  EXPECT_EQ(des[0].first, QueryType::currencies);
  EXPECT_EQ(des[0].second.duration, std::chrono::seconds(6));
  EXPECT_EQ(des[1].first, QueryType::allOrderBooks);
  EXPECT_EQ(des[1].second.duration, std::chrono::seconds(1));
  EXPECT_EQ(des[2].first, QueryType::currencyInfo);
  EXPECT_EQ(des[2].second.duration, std::chrono::seconds(3));
}

}  // namespace cct::schema