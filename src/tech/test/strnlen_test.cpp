#include "strnlen.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(strnlen, strnlen) {
  EXPECT_EQ(strnlen("123456789", 2), 2);
  EXPECT_EQ(strnlen("123456789", 3), 3);
  EXPECT_EQ(strnlen("123456789", 12), 9);
}

}  // namespace cct