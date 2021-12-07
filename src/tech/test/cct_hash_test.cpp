#include "cct_hash.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(SSPHashTest, HashValue) {
  for (int i = 0; i < 100; ++i) {
    EXPECT_NE(HashValue64(i), HashValue64(i + 1));
  }
}

TEST(SSPHashTest, HashCombine) {
  for (int i = 0; i < 20; ++i) {
    for (int j = 500; j < 520; ++j) {
      EXPECT_NE(HashCombine(i, j), HashCombine(i, j + 1));
      EXPECT_NE(HashCombine(i, j), HashCombine(i + 1, j));
    }
  }
}

TEST(SSPHashTest, Pair) {
  using T = std::pair<int64_t, uint8_t>;

  EXPECT_EQ(HashTuple()(T(37, 20)), HashTuple()(T(37, 20)));
  EXPECT_NE(HashTuple()(T(37, 36)), HashTuple()(T(37, 200)));
  EXPECT_NE(HashTuple()(T(37, 200)), HashTuple()(T(42, 200)));
}

}  // namespace cct