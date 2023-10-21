#include "gethostname.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(GetHostNameTest, Default) {
  HostNameGetter hostNameGetter;
  auto currentHostName = hostNameGetter.getHostName();

  EXPECT_FALSE(currentHostName.empty());
  EXPECT_EQ(currentHostName.find('\0'), string::npos);
}
}  // namespace cct