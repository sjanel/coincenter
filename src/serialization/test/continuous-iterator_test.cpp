#include "continuous-iterator.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(ContinuousIterator, UniqueElement) {
  ContinuousIterator it(1, 1);

  EXPECT_TRUE(it.hasNext());
  EXPECT_EQ(it.next(), 1);
  EXPECT_FALSE(it.hasNext());
}

TEST(ContinuousIterator, SeveralElements) {
  ContinuousIterator it(1, 3);

  EXPECT_TRUE(it.hasNext());
  EXPECT_EQ(it.next(), 1);
  EXPECT_TRUE(it.hasNext());
  EXPECT_EQ(it.next(), 2);
  EXPECT_TRUE(it.hasNext());
  EXPECT_EQ(it.next(), 3);
  EXPECT_FALSE(it.hasNext());
}

TEST(ContinuousIterator, Reverse) {
  ContinuousIterator it(1, -2);

  EXPECT_TRUE(it.hasNext());
  EXPECT_EQ(it.next(), 1);
  EXPECT_TRUE(it.hasNext());
  EXPECT_EQ(it.next(), 0);
  EXPECT_TRUE(it.hasNext());
  EXPECT_EQ(it.next(), -1);
  EXPECT_TRUE(it.hasNext());
  EXPECT_EQ(it.next(), -2);
  EXPECT_FALSE(it.hasNext());
}

}  // namespace cct
