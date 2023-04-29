#include "recentdeposit.hpp"

#include <gtest/gtest.h>

namespace cct {

inline bool operator==(const RecentDeposit &lhs, const RecentDeposit &rhs) {
  return lhs.amount() == rhs.amount() && lhs.timePoint() == rhs.timePoint();
}

using RecentDepositVector = RecentDeposit::RecentDepositVector;

class RecentDepositTest : public ::testing::Test {
 protected:
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

  void setRecentDepositsSameAmount() {
    recentDeposits = RecentDepositVector{RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::days(4)),
                                         RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::days(3)),
                                         RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::hours(50)),
                                         RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::hours(52))};
  }

  void setRecentDepositsDifferentAmounts() {
    recentDeposits = RecentDepositVector{RecentDeposit(MonetaryAmount(37), refTimePoint - std::chrono::seconds(6)),
                                         RecentDeposit(MonetaryAmount("37.5"), refTimePoint - std::chrono::hours(2)),
                                         RecentDeposit(MonetaryAmount(32), refTimePoint - std::chrono::hours(8)),
                                         RecentDeposit(MonetaryAmount(32), refTimePoint - std::chrono::hours(1)),
                                         RecentDeposit(MonetaryAmount(38), refTimePoint - std::chrono::hours(12)),
                                         RecentDeposit(MonetaryAmount(38), refTimePoint - std::chrono::hours(1)),
                                         RecentDeposit(MonetaryAmount(33), refTimePoint - std::chrono::minutes(1)),
                                         RecentDeposit(MonetaryAmount("33.1"), refTimePoint - std::chrono::minutes(12)),
                                         RecentDeposit(MonetaryAmount("27.5"), refTimePoint - std::chrono::days(3))};
  }

  TimePoint refTimePoint{Clock::now()};
  RecentDepositVector recentDeposits;
};

TEST_F(RecentDepositTest, Empty) {
  testNull(MonetaryAmount(0));
  testNull(MonetaryAmount(12));
}

TEST_F(RecentDepositTest, ExactAmount) {
  setRecentDepositsSameAmount();
  testNull(MonetaryAmount(10));
  RecentDeposit newDeposit(MonetaryAmount(10), refTimePoint - std::chrono::hours(20));
  recentDeposits.push_back(newDeposit);
  testExpected(MonetaryAmount(10), newDeposit);
}

TEST_F(RecentDepositTest, CloseAmount) {
  setRecentDepositsSameAmount();
  testNull(MonetaryAmount("10.001"));
  RecentDeposit newDeposit(MonetaryAmount(10), refTimePoint - std::chrono::hours(30));
  recentDeposits.push_back(newDeposit);
  testNull(MonetaryAmount("10.001"));
  newDeposit = RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::hours(20));
  recentDeposits.push_back(newDeposit);
  testExpected(MonetaryAmount("10.001"), newDeposit);
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositExactAmount1) {
  setRecentDepositsDifferentAmounts();
  testExpected(MonetaryAmount("37.5"), RecentDeposit(MonetaryAmount("37.5"), refTimePoint - std::chrono::hours(2)));
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositExactAmount2) {
  setRecentDepositsDifferentAmounts();
  testExpected(MonetaryAmount(32), RecentDeposit(MonetaryAmount(32), refTimePoint - std::chrono::hours(1)));
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositExactAmountButTooOld) {
  setRecentDepositsDifferentAmounts();
  testNull(MonetaryAmount("27.5"));
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositCloseToAmount1) {
  setRecentDepositsDifferentAmounts();
  testExpected(MonetaryAmount("37.501"), RecentDeposit(MonetaryAmount("37.5"), refTimePoint - std::chrono::hours(2)));
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositCloseToAmount2) {
  setRecentDepositsDifferentAmounts();
  testNull(MonetaryAmount("33.06"));
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositCloseToAmount3) {
  setRecentDepositsDifferentAmounts();
  testExpected(MonetaryAmount("33.0998"),
               RecentDeposit(MonetaryAmount("33.1"), refTimePoint - std::chrono::minutes(12)));
}

}  // namespace cct