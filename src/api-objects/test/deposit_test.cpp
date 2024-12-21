#include "deposit.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>

#include "cct_vector.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {
class DepositTest : public ::testing::Test {
 protected:
  TimePoint tp1{milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{milliseconds{std::numeric_limits<int64_t>::max() / 9000000}};
  TimePoint tp3{milliseconds{std::numeric_limits<int64_t>::max() / 8000000}};
  TimePoint tp4{milliseconds{std::numeric_limits<int64_t>::max() / 7000000}};

  Deposit deposit1{"id1", tp2, MonetaryAmount("0.045", "BTC"), Deposit::Status::success};
  Deposit deposit2{"id2", tp4, MonetaryAmount(37, "XRP"), Deposit::Status::success};
  Deposit deposit3{"id3", tp1, MonetaryAmount("15020.67", "EUR"), Deposit::Status::failed};
  Deposit deposit4{"id4", tp4, MonetaryAmount("1.31", "ETH"), Deposit::Status::processing};
  Deposit deposit5{"id5", tp3, MonetaryAmount("69204866.9", "DOGE"), Deposit::Status::success};

  vector<Deposit> deposits{deposit1, deposit2, deposit3, deposit4, deposit5};
};

TEST_F(DepositTest, SortByTimeFirst) {
  std::ranges::sort(deposits);

  EXPECT_EQ(deposits.front(), deposit3);
  EXPECT_EQ(deposits.back(), deposit4);
}
}  // namespace cct