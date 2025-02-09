#include "cct_exception.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(CctExceptionTest, InfoTakenFromConstCharStar) {
  EXPECT_STREQ(exception("This string can fill the inline storage").what(), "This string can fill the inline storage");
}

TEST(CctExceptionTest, FormatUntruncated) {
  EXPECT_STREQ(exception("Unknown currency code {}. Please enter a real one.", "BABYDOGE").what(),
               "Unknown currency code BABYDOGE. Please enter a real one.");
}

TEST(CctExceptionTest, FormatTruncated) {
  EXPECT_STREQ(exception("This is a {} that will not {} and it will be {} because it's too {}", "string",
                         "fit inside the buffer", "truncated", "long")
                   .what(),
               "This is a string that will not fit inside the buffer and it will be truncated becaus...");
}

}  // namespace cct