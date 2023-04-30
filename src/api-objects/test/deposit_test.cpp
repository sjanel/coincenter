#include "deposit.hpp"

#include "cct_vector.hpp"
#include "gtest/gtest.h"

namespace cct {
class DepositTest : public ::testing::Test {
 protected:
  TimePoint tp1{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 9000000}};
  TimePoint tp3{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 8000000}};
  TimePoint tp4{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 7000000}};

  Deposit deposit1{"id1", tp2, MonetaryAmount("0.045", "BTC"), Deposit::Status::kSuccess};
  Deposit deposit2{"id2", tp4, MonetaryAmount(37, "XRP"), Deposit::Status::kSuccess};
  Deposit deposit3{"id3", tp1, MonetaryAmount("15020.67", "EUR"), Deposit::Status::kFailureOrRejected};
  Deposit deposit4{"id4", tp4, MonetaryAmount("1.31", "ETH"), Deposit::Status::kProcessing};
  Deposit deposit5{"id5", tp3, MonetaryAmount("69204866.9", "DOGE"), Deposit::Status::kSuccess};

  vector<Deposit> deposits{deposit1, deposit2, deposit3, deposit4, deposit5};
};

TEST_F(DepositTest, SortByTimeFirst) {
  std::ranges::sort(deposits);
  EXPECT_EQ(deposits.front(), deposit3);
  EXPECT_EQ(deposits.back(), deposit4);
}
}  // namespace cct