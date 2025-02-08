#include "toupperlower-string.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(ToUpperLowerStringTest, ToUpperTest) {
  EXPECT_EQ(ToUpper("hello"), "HELLO");
  EXPECT_EQ(ToUpper("Hello"), "HELLO");
  EXPECT_EQ(ToUpper("HELLO"), "HELLO");
  EXPECT_EQ(ToUpper("hElLo"), "HELLO");
  EXPECT_EQ(ToUpper(" "), " ");
  EXPECT_EQ(ToUpper(""), "");
}

TEST(ToUpperLowerStringTest, ToLowerTest) {
  EXPECT_EQ(ToLower("hello"), "hello");
  EXPECT_EQ(ToLower("Hello"), "hello");
  EXPECT_EQ(ToLower("HELLO"), "hello");
  EXPECT_EQ(ToLower("hElLo"), "hello");
  EXPECT_EQ(ToLower(" "), " ");
  EXPECT_EQ(ToLower(""), "");
}

}  // namespace cct