#include "withdraw.hpp"

#include "cct_vector.hpp"
#include "gtest/gtest.h"

namespace cct {
class WithdrawTest : public ::testing::Test {
 protected:
  TimePoint tp1{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 9000000}};
  TimePoint tp3{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 8000000}};
  TimePoint tp4{std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 7000000}};

  Withdraw withdraw1{"id1", tp2, MonetaryAmount("0.045", "BTC"), Withdraw::Status::kSuccess,
                     MonetaryAmount("0.001", "BTC")};
  Withdraw withdraw2{"id2", tp4, MonetaryAmount(37, "XRP"), Withdraw::Status::kSuccess, MonetaryAmount("0.02 XRP")};
  Withdraw withdraw3{"id3", tp1, MonetaryAmount("15020.67", "EUR"), Withdraw::Status::kFailureOrRejected,
                     MonetaryAmount("0 EUR")};
  Withdraw withdraw4{"id4", tp4, MonetaryAmount("1.31", "ETH"), Withdraw::Status::kProcessing,
                     MonetaryAmount("0.001", "ETH")};
  Withdraw withdraw5{"id5", tp3, MonetaryAmount("69204866.9", "DOGE"), Withdraw::Status::kSuccess,
                     MonetaryAmount("1 DOGE")};

  vector<Withdraw> withdraws{withdraw1, withdraw2, withdraw3, withdraw4, withdraw5};
};

TEST_F(WithdrawTest, SortByTimeFirst) {
  std::ranges::sort(withdraws);
  EXPECT_EQ(withdraws.front(), withdraw3);
  EXPECT_EQ(withdraws.back(), withdraw4);
}

}  // namespace cct