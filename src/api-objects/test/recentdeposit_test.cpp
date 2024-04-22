#include "recentdeposit.hpp"

#include <gtest/gtest.h>

#include <chrono>

#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct::api {

inline bool operator==(const RecentDeposit &lhs, const RecentDeposit &rhs) {
  return lhs.amount() == rhs.amount() && lhs.timePoint() == rhs.timePoint();
}

class RecentDepositTest : public ::testing::Test {
 protected:
  void expectNotFound(MonetaryAmount ma) {
    RecentDeposit expectedDeposit(ma, refTimePoint);
    EXPECT_EQ(closestRecentDepositPicker.pickClosestRecentDepositPos(expectedDeposit), -1);
  }

  void expectFound(MonetaryAmount ma, int expectedDepositPos) {
    RecentDeposit tested(ma, refTimePoint);
    EXPECT_EQ(closestRecentDepositPicker.pickClosestRecentDepositPos(tested), expectedDepositPos);
  }

  void setRecentDepositsSameAmount() {
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::days(4)));
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::days(3)));
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::hours(50)));
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::hours(52)));
  }

  void setRecentDepositsDifferentAmounts() {
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(37), refTimePoint - seconds(6)));
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount("37.5"), refTimePoint - std::chrono::hours(2)));
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(32), refTimePoint - std::chrono::hours(8)));
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(32), refTimePoint - std::chrono::hours(1)));
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(38), refTimePoint - std::chrono::hours(12)));
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(38), refTimePoint - std::chrono::hours(1)));
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(33), refTimePoint - std::chrono::minutes(1)));
    closestRecentDepositPicker.push_back(
        RecentDeposit(MonetaryAmount("33.1"), refTimePoint - std::chrono::minutes(12)));
    closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount("27.5"), refTimePoint - std::chrono::days(3)));
  }

  TimePoint refTimePoint{Clock::now()};
  ClosestRecentDepositPicker closestRecentDepositPicker;
};

TEST_F(RecentDepositTest, Empty) {
  expectNotFound(MonetaryAmount(0));
  expectNotFound(MonetaryAmount(12));
}

TEST_F(RecentDepositTest, ExactAmount) {
  setRecentDepositsSameAmount();
  expectNotFound(MonetaryAmount(10));
  RecentDeposit newDeposit(MonetaryAmount(10), refTimePoint - std::chrono::hours(20));
  closestRecentDepositPicker.push_back(newDeposit);
  expectFound(MonetaryAmount(10), 4);
}

TEST_F(RecentDepositTest, CloseAmount) {
  setRecentDepositsSameAmount();
  expectNotFound(MonetaryAmount("10.001"));
  closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::hours(30)));
  expectNotFound(MonetaryAmount("10.001"));
  closestRecentDepositPicker.push_back(RecentDeposit(MonetaryAmount(10), refTimePoint - std::chrono::hours(20)));
  expectFound(MonetaryAmount("10.001"), 5);
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositExactAmount1) {
  setRecentDepositsDifferentAmounts();
  expectFound(MonetaryAmount("37.5"), 1);
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositExactAmount2) {
  setRecentDepositsDifferentAmounts();
  expectFound(MonetaryAmount(32), 3);
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositExactAmountButTooOld) {
  setRecentDepositsDifferentAmounts();
  expectNotFound(MonetaryAmount("27.5"));
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositCloseToAmount1) {
  setRecentDepositsDifferentAmounts();
  expectFound(MonetaryAmount("37.501"), 1);
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositCloseToAmount2) {
  setRecentDepositsDifferentAmounts();
  expectNotFound(MonetaryAmount("33.06"));
}

TEST_F(RecentDepositTest, SelectClosestRecentDepositCloseToAmount3) {
  setRecentDepositsDifferentAmounts();
  expectFound(MonetaryAmount("33.0998"), 7);
}

}  // namespace cct::api