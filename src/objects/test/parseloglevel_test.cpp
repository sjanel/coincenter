#include "parseloglevel.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"

namespace cct {
TEST(ParseLogLevel, InvalidLogName) { EXPECT_THROW(LogPosFromLogStr("invalid"), exception); }

TEST(ParseLogLevel, ValidLogName) {
  EXPECT_EQ(LogPosFromLogStr("off"), 0);
  EXPECT_EQ(LogPosFromLogStr("critical"), 1);
  EXPECT_EQ(LogPosFromLogStr("trace"), 6);
}
}  // namespace cct
