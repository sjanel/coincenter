#include "recentdeposit.hpp"

#include <gtest/gtest.h>

namespace cct {

inline bool operator==(const RecentDeposit &lhs, const RecentDeposit &rhs) {
  return lhs.amount() == rhs.amount() && lhs.timePoint() == rhs.timePoint();
}

using RecentDepositVector = RecentDeposit::RecentDepositVector;

TEST(RecentDepositTestEmpty, Empty) {
  RecentDeposit tested(MonetaryAmount(0), Clock::now());
  RecentDepositVector emptyDeposits;
  EXPECT_EQ(tested.selectClosestRecentDeposit(emptyDeposits), nullptr);
}

class RecentDepositTest : public ::testing::Test {
 protected:
  RecentDepositTest()
      : refTimePoint(Clock::now()),
        recentDeposits{RecentDeposit(MonetaryAmount(37), refTimePoint - std::chrono::seconds(2)),
                       RecentDeposit(MonetaryAmount("37.5"), refTimePoint - std::chrono::hours(2)),
                       RecentDeposit(MonetaryAmount(32), refTimePoint - std::chrono::hours(8)),
                       RecentDeposit(MonetaryAmount(32), refTimePoint - std::chrono::hours(1)),
                       RecentDeposit(MonetaryAmount(38), refTimePoint - std::chrono::hours(12)),
                       RecentDeposit(MonetaryAmount(38), refTimePoint - std::chrono::hours(1)),
                       RecentDeposit(MonetaryAmount(33), refTimePoint - std::chrono::minutes(1)),
                       RecentDeposit(MonetaryAmount("33.1"), refTimePoint - std::chrono::minutes(12)),
                       RecentDeposit(MonetaryAmount("27.5"), refTimePoint - std::chrono::days(4))} {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  void testNull(MonetaryAmount ma) {
    RecentDeposit tested(ma, refTimePoint);
    EXPECT_EQ(tested.selectClosestRecentDeposit(recentDeposits), nullptr);
  }

  void testExpected(MonetaryAmount ma, const RecentDeposit &expected) {
    RecentDeposit tested(ma, refTimePoint);
    const RecentDeposit *pRes = tested.selectClosestRecentDeposit(recentDeposits);
    ASSERT_NE(pRes, nullptr);
    EXPECT_EQ(*pRes, expected);
  }

  TimePoint refTimePoint;
  RecentDepositVector recentDeposits;
};

TEST_F(RecentDepositTest, SelectClosestRecentDepositExactAmount1) {
  testExpected(MonetaryAmount("37.5"), RecentDeposit(MonetaryAmount("37.5"), refTimePoint - std::chrono::hours(2)));
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositExactAmount2) {
  testExpected(MonetaryAmount(32), RecentDeposit(MonetaryAmount(32), refTimePoint - std::chrono::hours(1)));
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositExactAmountButTooOld) { testNull(MonetaryAmount("27.5")); }

TEST_F(RecentDepositTest, SelectClosestRecentDepositCloseToAmount1) {
  testExpected(MonetaryAmount("37.501"), RecentDeposit(MonetaryAmount("37.5"), refTimePoint - std::chrono::hours(2)));
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositCloseToAmount2) { testNull(MonetaryAmount("33.06")); }

TEST_F(RecentDepositTest, SelectClosestRecentDepositCloseToAmount3) {
  testExpected(MonetaryAmount("33.0998"),
               RecentDeposit(MonetaryAmount("33.1"), refTimePoint - std::chrono::minutes(12)));
}

}  // namespace cct