#include "serialization-tools.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(SerializationTools, ComputeProtoFileName) {
  EXPECT_EQ(ComputeProtoFileName(0), "00:00:00_00:59:59.binpb");
  EXPECT_EQ(ComputeProtoFileName(4), "04:00:00_04:59:59.binpb");
  EXPECT_EQ(ComputeProtoFileName(17), "17:00:00_17:59:59.binpb");
  EXPECT_EQ(ComputeProtoFileName(23), "23:00:00_23:59:59.binpb");
}
}  // namespace cct