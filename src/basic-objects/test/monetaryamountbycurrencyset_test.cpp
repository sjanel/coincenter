#include "monetaryamountbycurrencyset.hpp"

#include <gtest/gtest.h>

#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {

class MonetaryAmountByCurrencySetTest : public ::testing::Test {
 protected:
  MonetaryAmountByCurrencySet set{MonetaryAmount("1.5 EUR"), MonetaryAmount("2.5 DOGE"), MonetaryAmount("3.5 BTC")};
};

TEST_F(MonetaryAmountByCurrencySetTest, InsertOrAssign) {
  set.insert_or_assign(MonetaryAmount("2.5 EUR"));
  auto it = set.find(CurrencyCode("EUR"));
  ASSERT_NE(it, set.end());
  EXPECT_EQ(*it, MonetaryAmount("2.5 EUR"));
}

TEST_F(MonetaryAmountByCurrencySetTest, InsertOrAssignRange) {
  MonetaryAmountByCurrencySet other{MonetaryAmount("4 ETH"), MonetaryAmount("5.5 DOGE"), MonetaryAmount("6.5 POW")};
  set.insert_or_assign(other.begin(), other.end());  // Inserting the same currency as in set should overwrite the value

  EXPECT_EQ(set.size(), 5U);

  MonetaryAmountByCurrencySet expected{MonetaryAmount("1.5 EUR"), MonetaryAmount("4 ETH"), MonetaryAmount("5.5 DOGE"),
                                       MonetaryAmount("6.5 POW"), MonetaryAmount("3.5 BTC")};

  EXPECT_EQ(set, expected);
}

}  // namespace cct