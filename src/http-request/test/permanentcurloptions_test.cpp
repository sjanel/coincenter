#include "permanentcurloptions.hpp"

#include <gtest/gtest.h>

#include "cct_string.hpp"

namespace cct {
TEST(PermanentCurlOptions, Builder) {
  auto permanentCurlOptions = PermanentCurlOptions::Builder().setAcceptedEncoding("SomeEncoding").build();
  EXPECT_EQ(permanentCurlOptions.getAcceptedEncoding(), string("SomeEncoding"));
}
}  // namespace cct