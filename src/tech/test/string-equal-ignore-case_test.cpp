#include "string-equal-ignore-case.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(CaseInsensitiveEqual, BasicTests) {
  EXPECT_FALSE(CaseInsensitiveEqual("hell", "HELLO"));
  EXPECT_TRUE(CaseInsensitiveEqual("hello", "HELLO"));
  EXPECT_TRUE(CaseInsensitiveEqual("Hello", "hElLo"));
  EXPECT_FALSE(CaseInsensitiveEqual("hello", "world"));
  EXPECT_TRUE(CaseInsensitiveEqual("", ""));
}

}  // namespace cct