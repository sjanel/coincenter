#include "overflow-check.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace cct {
TEST(WillSumOverflowTest, Int8) {
  EXPECT_FALSE(WillSumOverflow(int8_t{}, int8_t{}));
  EXPECT_FALSE(WillSumOverflow(int8_t{1}, int8_t{13}));
  EXPECT_FALSE(WillSumOverflow(int8_t{-45}, int8_t{89}));
  EXPECT_FALSE(WillSumOverflow(int8_t{-87}, int8_t{-25}));
  EXPECT_FALSE(WillSumOverflow(int8_t{74}, int8_t{50}));

  EXPECT_TRUE(WillSumOverflow(int8_t{10}, int8_t{125}));
  EXPECT_TRUE(WillSumOverflow(int8_t{-45}, int8_t{-89}));
  EXPECT_TRUE(WillSumOverflow(int8_t{-87}, int8_t{-45}));
  EXPECT_TRUE(WillSumOverflow(int8_t{74}, int8_t{60}));
}
}  // namespace cct