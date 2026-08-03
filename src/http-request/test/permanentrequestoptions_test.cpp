#include "permanentrequestoptions.hpp"

#include <gtest/gtest.h>

#include "cct_string.hpp"

namespace cct {
TEST(PermanentRequestOptions, Builder) {
  auto permanentHttpRequestOptions = PermanentRequestOptions::Builder().setAcceptedEncoding("SomeEncoding").build();
  EXPECT_EQ(permanentHttpRequestOptions.getAcceptedEncoding(), string("SomeEncoding"));
}
}  // namespace cct